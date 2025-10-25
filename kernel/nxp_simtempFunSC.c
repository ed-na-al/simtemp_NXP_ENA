#include <linux/init.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <linux/random.h>
#include <linux/device.h>
#include <linux/stat.h>
#include <linux/poll.h>   
#include <linux/wait.h>   
#include <linux/spinlock.h>
#include <linux/kfifo.h>  
#include <linux/ktime.h>     
#include <linux/timekeeping.h> 

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Eduardo Naranjo Alvarado");
MODULE_LICENSE("Dual BSD/GPL");

#define DEVICE_NAME "simtemp0"		// Name of file device
#define CLASS_NAME  "simtemp"		// Name of class device
#define MODULE_NAME "nxp_simtemp"	// Name of module device

#define SAMPLE_FIFO_SIZE 1  		// Number of samples stored
#define DEFAULT_SAMPLING_MS 5000	// Default number of sampling
#define DEFAULT_THRESHOLD_mC 45000	// Default number of threshold

/* flags */
#define FLAG_NEW_SAMPLE        (1U << 0)	// 0b00000001
#define FLAG_THRESHOLD_CROSSED (1U << 1)	// 0b00000010

/* 
 * Global Variables 
 */

int simtemp_major 		   =	0;	// Major number
unsigned int simtemp_minor = 	0;	// Minor number

/*
 * =======================================================
 * 						STRUCTURES
 * =======================================================
 */
 
 struct simtemp_sample {
    __u64 timestamp_ns; // ktime_get_real_ns() o ktime_get_ns()
    __s32 temp_mC;      // milli-degrees Celsius
    __u32 flags;        // bit0 NEW_SAMPLE, bit1 THRESHOLD
} __attribute__((packed));

struct simtemp_dev
{
	struct semaphore sem;	  			// Mutual exclusion semaphore
	struct cdev cdev;	  				// Char device structure
	struct device *dev;	  				// Device structure /dev
	int simtemp; 		  				// Sim Temperature
	int sampling_ms;	  				// Sample Frecuency
	int threshold_mc;	  				// Threshold in mc
	unsigned long samples_taken;		// Samples taken
	char sensor_mode[16]; 				// Function mode
	wait_queue_head_t read_alert_wq;   	// wait queue para readers y alert (poll/wait) 
    spinlock_t fifo_lock;        		// protege kfifo 
    struct kfifo fifo;           		// FIFO de samples 
    bool alert_pending;          		// true si hay evento de prioridad (threshold) 
    spinlock_t state_lock;       		// protege alert_pending y counters 
	int count_alerts; 					// Count alerts of umbral
	struct delayed_work my_work_delay; 	// Work queue
 };

struct simtemp_dev simtemp_device = {
	.sampling_ms = DEFAULT_SAMPLING_MS,
	.threshold_mc = DEFAULT_THRESHOLD_mC,
	.sensor_mode = "normal",
}; // Allocate the devices 
	

static struct class *simtemp_class = NULL;		// Class struct
static struct device *simtemp_device_f = NULL;	// Device struct
static struct workqueue_struct *my_workqueue;	// Workqueue struct


/*
 * =======================================================
 * 						PROTOTYPE
 * =======================================================
 */

int simtemp_open(struct inode *inode, struct file *filp);
int simtemp_release(struct inode *inode, struct file *filp);
unsigned int simtemp_poll(struct file *file, poll_table *wait);
ssize_t simtemp_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
static void simtemp_setup_cdev(struct simtemp_dev *dev, int index);
static void workqueue_function(struct work_struct *work);
u32 generate_temperature_sample(struct simtemp_dev *sdev);


/*
 * =======================================================
 * 					F_OPS STRUCTURE
 * =======================================================
 */

struct file_operations simtemp_fops =
{
	.owner = THIS_MODULE,
	.read = simtemp_read,
	.poll = simtemp_poll,
	.open = simtemp_open,
	.release = simtemp_release,
};


/* 
 * =======================================================
 * 			FUNCTIONS SYSFS (SHOW / STORE)
 * =======================================================
 */

/* 
 * SAMPLING_MS
 */

static ssize_t sampling_ms_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct simtemp_dev *sdev = dev_get_drvdata(dev);
    int local_sampling_ms; 
    unsigned long flags;
    
    // Mostrar el periodo de la toma de muestras
    spin_lock_irqsave(&sdev->state_lock, flags);
    local_sampling_ms = sdev->sampling_ms;
    spin_unlock_irqrestore(&sdev->state_lock, flags);
    
    return sprintf(buf, "%d\n", local_sampling_ms);
}

static ssize_t sampling_ms_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct simtemp_dev *sdev = dev_get_drvdata(dev);
    int value = 0;
	unsigned long flags;

    if (kstrtoint(buf, 10, &value))	  // Pasar de string a int
        return -EINVAL;

    if (value < 10 || value > 10000)  // rango de 10ms a 10s por ejemplo
        return -EINVAL;
	
    spin_lock_irqsave(&sdev->state_lock, flags);
    sdev->sampling_ms = value;
    spin_unlock_irqrestore(&sdev->state_lock, flags);
    pr_info("SimTemp: Nueva frecuencia de muestreo %d ms\n", value);
    
    return count;
}

static DEVICE_ATTR_RW(sampling_ms);

/* 
 * THRESHOLD
 */

static ssize_t threshold_mc_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct simtemp_dev *sdev = dev_get_drvdata(dev);
    int local_threshold_mc;
    unsigned long flags;
    
    // Leer el valor de umbral
	spin_lock_irqsave(&sdev->state_lock, flags);
    local_threshold_mc = sdev->threshold_mc;
    spin_unlock_irqrestore(&sdev->state_lock, flags);
    
    return sprintf(buf, "%d\n", local_threshold_mc);
}

static ssize_t threshold_mc_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct simtemp_dev *sdev = dev_get_drvdata(dev);
    int value = 0;
    unsigned long flags;
	
	// El valor debe estar en m°C
    if (kstrtoint(buf, 10, &value))
        return -EINVAL;
	// Copiar el valor de umbral
    spin_lock_irqsave(&sdev->state_lock, flags);
    sdev->threshold_mc = value;
    spin_unlock_irqrestore(&sdev->state_lock, flags);
    
    pr_info("SimTemp: Nuevo umbral de alerta %d m°C\n", value);
    return count;
}

static DEVICE_ATTR_RW(threshold_mc);

/* 
 * STATS
 */

static ssize_t stats_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct simtemp_dev *sdev = dev_get_drvdata(dev);
	// Variables locales para copiar los datos de forma segura
    int local_sampling, local_threshold, local_alerts;
    unsigned long local_samples;
    char local_mode[16];
    unsigned long flags;
    
    // Proteger las variables a leer
    spin_lock_irqsave(&sdev->state_lock, flags);

    local_sampling = sdev->sampling_ms;
    local_threshold = sdev->threshold_mc;
    local_samples = sdev->samples_taken;
    strncpy(local_mode, sdev->sensor_mode, sizeof(local_mode) - 1);
    local_alerts = sdev->count_alerts;
    
    spin_unlock_irqrestore(&sdev->state_lock, flags);
    
    local_mode[sizeof(local_mode) - 1] = '\0'; // Asegurar null
    
    // Retornar las stats
    return sprintf(buf,
        "Frecuencia de muestreo: %d ms\n"
        "Umbral: %d m°C\n"
        "Muestras tomadas: %lu\n"
        "Modo de sensor: %s\n"
        "Cuentas a alertas: %d\n",
		local_sampling,
        local_threshold,
        local_samples,
        local_mode,
        local_alerts);
}

static DEVICE_ATTR_RO(stats);

/* 
 * MODE
 */

static ssize_t mode_show(struct device *dev, struct device_attribute *attr, char *buf) {
    struct simtemp_dev *sdev = dev_get_drvdata(dev);
    char local_mode[16];
    unsigned long flags;
    
    // Leer el valor de mode
	spin_lock_irqsave(&sdev->state_lock, flags);
    strncpy(local_mode, sdev->sensor_mode, sizeof(local_mode) - 1);
    spin_unlock_irqrestore(&sdev->state_lock, flags);
    
    return sprintf(buf, "%s\n", local_mode); // Mostrar el modo actual
}

static ssize_t mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
    struct simtemp_dev *sdev = dev_get_drvdata(dev);
    unsigned long flags;

    // Actualizar el valor de 'mode'
    if (strncmp(buf, "normal", 6) == 0) {
		spin_lock_irqsave(&sdev->state_lock, flags);
        strncpy(sdev->sensor_mode, "normal", sizeof(sdev->sensor_mode) - 1); // Deja espacio para '\0'
        spin_unlock_irqrestore(&sdev->state_lock, flags);
    }
    else if (strncmp(buf, "noisy", 5) == 0) {
		spin_lock_irqsave(&sdev->state_lock, flags);
        strncpy(sdev->sensor_mode, "noisy", sizeof(sdev->sensor_mode) - 1);
        spin_unlock_irqrestore(&sdev->state_lock, flags);
    }
    else if (strncmp(buf, "ramp", 4) == 0) {
		spin_lock_irqsave(&sdev->state_lock, flags);
        strncpy(sdev->sensor_mode, "ramp", sizeof(sdev->sensor_mode) - 1);
        spin_unlock_irqrestore(&sdev->state_lock, flags);
    }
    else {
        return -EINVAL; // Valor inválido
    }

    // Asegurarse de que la cadena esté terminada en '\0'
    sdev->sensor_mode[sizeof(sdev->sensor_mode) - 1] = '\0';

    return count; // Retorna el número de bytes escritos
}

static DEVICE_ATTR_RW(mode); // Exponer 'mode' en sysfs


/*
 * =======================================================
 * 					FILE OPERATIONS FUNCTIONS
 * =======================================================
 */
 
/*
 * OPEN FUNCTION
 */

int simtemp_open(struct inode *inode, struct file *flip)
{	
	struct simtemp_dev *dev; // Device information

	dev = container_of(inode->i_cdev, struct simtemp_dev, cdev);
	flip->private_data = dev; // Preserving state information
	
	return(0); 
}

/*
 * READ FUNCTION
 */

ssize_t simtemp_read(struct file *flip, char __user *buf, size_t count, loff_t *f_pos)
{
	struct simtemp_dev *dev = flip->private_data;	// Pointer to device (simtemp_dev structure)
	struct simtemp_sample bin_rec;
	int ret = 0;
	unsigned long flags, ret_kfifo = 0;
	

	// si len es mayor que count Actualizar len con count para pasar los datos que pide el ususario
	if(count < sizeof(bin_rec))
		return -EINVAL;
		
	/* block until data present or signal */
    ret = wait_event_interruptible(dev->read_alert_wq, !kfifo_is_empty(&dev->fifo) || signal_pending(current));
    if (ret)
        return ret; /* -ERESTARTSYS */
        
	/* pop one sample */
    spin_lock_irqsave(&dev->fifo_lock, flags);
    ret_kfifo = kfifo_out(&dev->fifo, &bin_rec, sizeof(bin_rec));
    if (ret_kfifo != sizeof(bin_rec)) {
        spin_unlock_irqrestore(&dev->fifo_lock, flags);
        return -EAGAIN; /* race */
    }
    spin_unlock_irqrestore(&dev->fifo_lock, flags);

	// Limpiar bandera de alerta activa
	if (dev->alert_pending) {
		spin_lock_irqsave(&dev->state_lock, flags);
		dev->alert_pending = false;
		spin_unlock_irqrestore(&dev->state_lock, flags);
	}

	// Copiar estructura a usuario
	if(copy_to_user(buf, &bin_rec, sizeof(bin_rec)))
		return -EFAULT;
	
	printk(KERN_ALERT "Read is made\n");

	return sizeof(bin_rec);
}

/*
 * POLL FUNCTION
 */

unsigned int simtemp_poll(struct file *file, poll_table *wait)
{
    struct simtemp_dev *dev = file->private_data; /* o como recuperes tu struct */
    unsigned int mask = 0;
    unsigned long flags;

    /* Registrar la wait queue para que poll la observe */
    poll_wait(file, &dev->read_alert_wq, wait);

    /* ¿Hay muestras disponibles? => POLLIN/POLLRDNORM */
    spin_lock_irqsave(&dev->fifo_lock, flags);
    if (!kfifo_is_empty(&dev->fifo))
        mask |= POLLIN | POLLRDNORM;
    spin_unlock_irqrestore(&dev->fifo_lock, flags);

    /* ¿Hay alerta pendiente? => POLLPRI (o POLLERR según convenga) */
    spin_lock_irqsave(&dev->state_lock, flags);
    if (dev->alert_pending)
        mask |= POLLPRI;
    spin_unlock_irqrestore(&dev->state_lock, flags);

    return mask;
}


/*
 * RELEASE FUNCTION
 */

int simtemp_release(struct inode *inode, struct file *flip)
{
	return(0);
}


/*
 * =======================================================
 * 					GENERATE TEMPERATURE
 * =======================================================
 */

u32 generate_temperature_sample(struct simtemp_dev *sdev) {
    u32 temp;
    
    // Verificar el modo del sensor
    if (strncmp(sdev->sensor_mode, "normal", 6) == 0) {
        // Generar temperatura normal
        temp = 25000; // Valor de ejemplo
    } else if (strncmp(sdev->sensor_mode, "noisy", 5) == 0) {
        // Generar temperatura con ruido
        temp = 25000 + (get_random_u32() % 1000); // Rango ruidoso
    } else if (strncmp(sdev->sensor_mode, "ramp", 4) == 0) {
        // Generar temperatura en rampa
        static u32 ramp_temp = 25000;
        ramp_temp += 10; // Aumentar gradualmente
        temp = ramp_temp;
    } else {
        // Modo desconocido, temperatura por defecto
        temp = 25000;
    }
    
    return temp;
}

static int simtemp_sample_enqueue(struct simtemp_dev *dev_s, struct simtemp_sample *sim_s)
{
    unsigned long flags;
    int ret;
    
	// Después de actualizar dev->simtemp
	spin_lock_irqsave(&dev_s->fifo_lock, flags);
	if(kfifo_is_full(&dev_s->fifo)){
		ret = -ENOSPC;
	} else{
		ret = kfifo_in(&dev_s->fifo, sim_s, sizeof(*sim_s)); // Si usas FIFO
	}
	spin_unlock_irqrestore(&dev_s->fifo_lock, flags);
	
	wake_up_interruptible(&dev_s->read_alert_wq);
	
	// Si la muestra cruza el umbral, marca alerta
	if (sim_s->flags & FLAG_THRESHOLD_CROSSED) {
		spin_lock_irqsave(&dev_s->state_lock, flags);
		dev_s->alert_pending = true;
		spin_unlock_irqrestore(&dev_s->state_lock, flags);
		// Despierta el proceso por alerta de umbral
		wake_up_interruptible(&dev_s->read_alert_wq);
		dev_s->count_alerts++;
	}

    return ret;
}


/*
 * =======================================================
 * 					WORKQUEUE FUNCTION
 * =======================================================
 */
static void workqueue_function(struct work_struct *work){
	
	//u32 random_temp = 0;
	int delay_ms = 5000, ret = 0;
	static unsigned long countSample = 0;
	struct simtemp_sample sim_s;
	
	// Cast to delayed_work
	struct delayed_work *dwork = to_delayed_work(work);
	
	// Puntero a la estructura simtemp_drivers donde esta contador
	struct simtemp_dev *dev = container_of(dwork, struct simtemp_dev, my_work_delay);
	
	u32 random_temp = generate_temperature_sample(dev);
	
	countSample++;
	dev->samples_taken = countSample;
	
	// Introducir a binary record
	sim_s.temp_mC = random_temp;
	sim_s.timestamp_ns = ktime_get_ns();
	sim_s.flags = FLAG_NEW_SAMPLE;
	
	// Si se supera el umbral establecer bandera
	if (sim_s.temp_mC > dev->threshold_mc){
		sim_s.flags = 0;
		sim_s.flags = FLAG_THRESHOLD_CROSSED;
	}
	
	// Introducir los valores de retorno en la FIFO
	ret = simtemp_sample_enqueue(dev, &sim_s);
	if(ret <= 0)
		pr_info("Full Queue");
		
	
	// Reprogramar delay para que sea periodico
	delay_ms = dev->sampling_ms;
	queue_delayed_work(my_workqueue, dwork, msecs_to_jiffies(delay_ms)); // cada 5s
	

	pr_info("Workqueue is runinig");

}

/*
 * =======================================================
 * 						SETUP CHAR DEVICE
 * =======================================================
 */
static void simtemp_setup_cdev(struct simtemp_dev *dev, int index)
{
	int err, devno = MKDEV(simtemp_major, simtemp_minor + index);

	cdev_init(&dev->cdev, &simtemp_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &simtemp_fops;
	err = cdev_add(&dev->cdev, devno, 1);
	
	if(err)
		printk(KERN_NOTICE "Error %d adding simtemp %d\n",err,index);
}
/*
 * =======================================================
 * 						INIT FUNCTION
 * =======================================================
 */

static int __init initialization_function(void)
{
	int result;
	// dev se utiliza para obtener el número mayor/menor
	dev_t devno = 0; 
	int ret_dv_file_sm, ret_dv_file_th, ret_dv_file_st, ret_dv_file_md;
	
	printk(KERN_ALERT "ENTRY TEST\n");

	// OBTENER MAYOR/MENOR (char dev region)
	result = alloc_chrdev_region(&devno, simtemp_minor, 1, MODULE_NAME);
	simtemp_major = MAJOR(devno);
	
	if (result < 0) {
		printk(KERN_WARNING "simtemp: can't get major %d\n", simtemp_major);
		goto fail_region; // Nada que limpiar antes de aquí
	}
	
	// INICIALIZAR ESTRUCTURA PRIVADA
	// Inicializar locks y waitqueues antes de usarlos en workqueue/sysfs
	sema_init(&simtemp_device.sem, 1);
	init_waitqueue_head(&simtemp_device.read_alert_wq);
	spin_lock_init(&simtemp_device.fifo_lock);
	spin_lock_init(&simtemp_device.state_lock);

	// ASIGNAR KFIFO
	// Asumimos que SAMPLE_FIFO_SIZE y struct simtemp_sample están definidos globalmente
	result = kfifo_alloc(&simtemp_device.fifo, SAMPLE_FIFO_SIZE * sizeof(struct simtemp_sample), GFP_KERNEL);
	if (result) {
		pr_err("SimTemp: Error al alocar kfifo\n");
		goto fail_region; // Limpiar solo el alloc_chrdev_region
	}
	
	// CREAR CDEV
	simtemp_setup_cdev(&simtemp_device, 0); // Asumimos que esto llama a cdev_init y cdev_add
	if (simtemp_device.cdev.owner != THIS_MODULE) { // Asumimos un check de error
		result = -ENOMEM; // Código de error genérico si cdev_add/init falla internamente
		goto fail_kfifo;
	}
	
	// CREAR WORKQUEUE
	my_workqueue = create_workqueue("my_workqueue");
	if (my_workqueue == NULL) {
		result = -ENOMEM;
		goto fail_cdev;
	}
	
	// INICIALIZAR Y ENCOLAR WORK (se limpiará con destroy_workqueue)
	INIT_DELAYED_WORK(&simtemp_device.my_work_delay, workqueue_function);
	// Enqueue task in 5 secs
	queue_delayed_work(my_workqueue, &simtemp_device.my_work_delay, msecs_to_jiffies(5000));	
	
	// CREAR CLASE /sys/class
	simtemp_class = class_create(CLASS_NAME);
	if (IS_ERR(simtemp_class)) {
		result = PTR_ERR(simtemp_class);
		pr_alert("tempsim: fallo al crear clase\n");	
		goto fail_workqueue;
 	}

	// CREAR DISPOSITIVO /dev/simtemp0
	simtemp_device_f = device_create(simtemp_class, NULL, MKDEV(simtemp_major, simtemp_minor), &simtemp_device, DEVICE_NAME);
	if (IS_ERR(simtemp_device_f)) {
		result = PTR_ERR(simtemp_device_f);
		pr_alert("tempsim: fallo al crear dispositivo\n");
		goto fail_class;
	}

	// ASOCIAR PUNTERO DE DATOS PRIVADOS
	dev_set_drvdata(simtemp_device_f, &simtemp_device);
		
	// CREAR ATRIBUTOS (sysfs files)
	ret_dv_file_sm = device_create_file(simtemp_device_f, &dev_attr_sampling_ms);
	ret_dv_file_th = device_create_file(simtemp_device_f, &dev_attr_threshold_mc);
	ret_dv_file_st = device_create_file(simtemp_device_f, &dev_attr_stats);
	ret_dv_file_md = device_create_file(simtemp_device_f, &dev_attr_mode);

	// Si falla la creación de algún atributo, debemos limpiar todo
	if (ret_dv_file_sm < 0 || ret_dv_file_th < 0 || ret_dv_file_st < 0 || ret_dv_file_md < 0) {
		pr_err("SimTemp: Fallo al crear atributos sysfs\n");
		result = -EINVAL; // O el código de error del primer fallo
		goto fail_attributes;
	}
	
	printk(KERN_INFO "SimTemp: Device initialized successfully\n");

	return 0; // Éxito total

	// --- SECCIÓN DE LIMPIEZA DE ERRORES (En orden inverso) ---

	fail_attributes:
		// Borrar solo los que se pudieron crear (aunque device_destroy a veces limpia esto)
		device_remove_file(simtemp_device_f, &dev_attr_sampling_ms);
		device_remove_file(simtemp_device_f, &dev_attr_threshold_mc);
		device_remove_file(simtemp_device_f, &dev_attr_stats);	
		device_remove_file(simtemp_device_f, &dev_attr_mode);
		device_destroy(simtemp_class, devno); // Devno es MKDEV(simtemp_major, simtemp_minor)

	fail_class:
		class_destroy(simtemp_class);

	fail_workqueue:
		// Cancelar cualquier trabajo pendiente y destruir la workqueue
		cancel_delayed_work_sync(&simtemp_device.my_work_delay);
		destroy_workqueue(my_workqueue);

	fail_cdev:
		cdev_del(&simtemp_device.cdev);

	fail_kfifo:
		kfifo_free(&simtemp_device.fifo); // liberar memoria alocada para kfifo

	fail_region:
		// Solo limpia si alloc_chrdev_region tuvo éxito
		if (simtemp_major != 0)
			unregister_chrdev_region(devno, 1);
		
		return result; // Retornar el código de error original

}


/*
 * =======================================================
 * 						EXIT FUNCTION
 * =======================================================
 */

static void __exit cleanup_function(void)
{
	dev_t devno = MKDEV(simtemp_major, simtemp_minor);

	// Delete device and unregister major, minor
	cdev_del(&simtemp_device.cdev);	
	unregister_chrdev_region(devno, 1);

	// Wait to sycronize the queue
	cancel_delayed_work_sync(&simtemp_device.my_work_delay);
	// Destroy the workqueue	
	destroy_workqueue(my_workqueue);
	
	// Delete class, device, device file
	// device_remove_file(simtemp_device_f, &dev_attr_simtemp);
	device_remove_file(simtemp_device_f, &dev_attr_sampling_ms);
	device_remove_file(simtemp_device_f, &dev_attr_threshold_mc);
	device_remove_file(simtemp_device_f, &dev_attr_stats);	
	device_remove_file(simtemp_device_f, &dev_attr_mode);
	device_destroy(simtemp_class, devno);
	class_unregister(simtemp_class);
	class_destroy(simtemp_class);
	
	// Free kfifo
    kfifo_free(&simtemp_device.fifo);


	printk(KERN_ALERT "EXIT TEST\n");
}



module_init(initialization_function);
module_exit(cleanup_function);

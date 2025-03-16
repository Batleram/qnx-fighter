#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/neutrino.h>
#include <sys/siginfo.h>
#include <sys/rpi_gpio.h>
#include <Python.h>
#include "py_pwm.h"

typedef struct constant constant_t;
typedef struct callback callback_t;

PyMODINIT_FUNC PyInit_rpi_gpio(void);

/**
 * Constant published by the module.
 */
struct constant
{
    PyObject    *obj;
    char const  *name;
    unsigned    value;
};

/**
 * Node in a list of callback functions.
 */
struct callback
{
    PyObject    *func;
    unsigned    gpio_num;
    callback_t  *next;
};

int             gpio_fd;

static PyObject *gpio_err;
static int      event_coid = -1;

/**
 * List of constants.
 */
static constant_t   constants[] = {
    { .name = "LOW",        .value = 0 },
    { .name = "HIGH",       .value = 1 },
    { .name = "IN",         .value = 0 },
    { .name = "OUT",        .value = 1 },
    { .name = "BOARD",      .value = 0 },
    { .name = "BCM",        .value = 1 },
    { .name = "PUD_OFF",    .value = 0 },
    { .name = "PUD_DOWN",   .value = 1 },
    { .name = "PUD_UP",     .value = 2 },
    { .name = "FALLING",    .value = RPI_EVENT_EDGE_FALLING },
    { .name = "RISING",     .value = RPI_EVENT_EDGE_RISING },
    { .name = "BOTH",       .value = RPI_EVENT_EDGE_FALLING | RPI_EVENT_EDGE_RISING },
};

/**
 * PIN numbers in board layout.
 */
static unsigned board_gpios[] = {
    [3] = 2,
    [5] = 3,
    [7] = 4,
    [8] = 14,
    [10] = 15,
    [11] = 17,
    [12] = 18,
    [13] = 27,
    [15] = 22,
    [16] = 23,
    [18] = 24,
    [19] = 10,
    [21] = 9,
    [22] = 25,
    [23] = 11,
    [24] = 8,
    [26] = 7,
    [29] = 5,
    [31] = 6,
    [32] = 12,
    [33] = 13,
    [35] = 19,
    [36] = 16,
    [37] = 26,
    [38] = 20,
    [40] = 21
};

/**
 * PIN numbers in BCM layout.
 */
static unsigned bcm_gpios[] = {
    [0] = 0,
    [1] = 1,
    [2] = 2,
    [4] = 4,
    [5] = 5,
    [14] = 14,
    [15] = 15,
    [17] = 17,
    [18] = 18,
    [27] = 27,
    [22] = 22,
    [23] = 23,
    [24] = 24,
    [10] = 10,
    [9] = 9,
    [25] = 25,
    [11] = 11,
    [8] = 8,
    [7] = 7,
    [5] = 5,
    [6] = 6,
    [12] = 12,
    [13] = 13,
    [19] = 19,
    [16] = 16,
    [26] = 26,
    [20] = 20,
    [21] = 21
};

static unsigned         *gpio_table = bcm_gpios;
static unsigned         gpio_table_max = 27;
static callback_t       *callback_table[RPI_GPIO_NUM];
static pthread_mutex_t  callback_table_lock = PTHREAD_MUTEX_INITIALIZER;

#define INVAL_GPIO  (unsigned)-1

/**
 * Convert a user's GPIO PIN number to the one used for calls to the resource
 * manager, based on the chosen layout.
 * @param   num User value
 * @return  Actual PIN number
 */
static inline unsigned
get_gpio(unsigned num)
{
    if (num > gpio_table_max) {
        return INVAL_GPIO;
    }

    return gpio_table[num];
}

static void
set_error(char const * const str, ...)
{
    static char err[256];
    va_list     args;

    va_start(args, str);
    vsnprintf(err, sizeof(err), str, args);
    va_end(args);

    PyErr_SetString(gpio_err, err);
}

static void
exec_cb(PyObject *func, unsigned gpio)
{
    PyGILState_STATE    gilst = PyGILState_Ensure();
    PyObject * const    rc = PyObject_CallFunction(func, "I", gpio);
    Py_XDECREF(rc);
    PyGILState_Release(gilst);
}

static void *
event_thread(void *arg)
{
    sem_t   *sem = arg;

    // Create a channel to receive notification pulses on.
    int const   chid = ChannelCreate(_NTO_CHF_PRIVATE);
    if (chid == -1) {
        sem_post(sem);
        return NULL;
    }

    // Connect to the channel in order to allow event delivery.
    int const   coid = ConnectAttach(0, 0, chid, _NTO_SIDE_CHANNEL, 0);
    if (coid == -1) {
        sem_post(sem);
        return NULL;
    }

    // Thread is ready, notify the main thread.
    event_coid = coid;
    sem_post(sem);

    for (;;) {
        // Wait for a pulse.
        struct _pulse   pulse;
        int const       rc = MsgReceivePulse(chid, &pulse, sizeof(pulse), NULL);

        if (rc == -1) {
            // FIXME:
            // Handle errors.
            return NULL;
        }

        if (pulse.code != _PULSE_CODE_MINAVAIL) {
            continue;
        }

        // Extract GPIO number.
        unsigned    gpio = pulse.value.sival_int;

        if (gpio >= RPI_GPIO_NUM) {
            continue;
        }

        // Run callbacks for this GPIO.
        pthread_mutex_lock(&callback_table_lock);

        callback_t  *cb = callback_table[pulse.value.sival_int];
        while (cb != NULL) {
            exec_cb(cb->func, cb->gpio_num);
            cb = cb->next;
        }

        pthread_mutex_unlock(&callback_table_lock);
    }
}

static int
init_event_thread()
{
    pthread_t   tid;
    sem_t       sem;

    sem_init(&sem, 0, 0);

    // Create the event receiving thread.
    if (pthread_create(&tid, NULL, event_thread, &sem) != 0) {
        set_error("Failed to create event thread");
        return 0;
    }

    // Wait for the thread to establish the self connection.
    sem_wait(&sem);
    if (event_coid == -1) {
        set_error("Failed to create event thread");
        return 0;
    }

    return 1;
}

static PyObject *
rpi_gpio_setup(PyObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = {
        "channel",
        "direction",
        "pull_up_down",
        NULL
    };

    unsigned    gpio;
    unsigned    value;
    unsigned    pud = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "II|I", kwlist,
                                     &gpio, &value, &pud)) {
        return NULL;
    }

    gpio = get_gpio(gpio);
    if (gpio == INVAL_GPIO) {
        set_error("Invalid GPIO number");
        return NULL;
    }

    rpi_gpio_msg_t  msg = {
        .hdr.type = _IO_MSG,
        .hdr.subtype = RPI_GPIO_SET_SELECT,
        .hdr.mgrid = RPI_GPIO_IOMGR,
        .gpio = gpio,
        .value = value
    };

    if (MsgSend(gpio_fd, &msg, sizeof(msg), &msg, sizeof(msg)) == -1) {
        set_error("RPI_GPIO_SET_SELECT: %s", strerror(errno));
        return NULL;
    }

    if (pud != 0) {
        msg.hdr.subtype = RPI_GPIO_PUD;
        msg.value = pud;

        if (MsgSend(gpio_fd, &msg, sizeof(msg), &msg, sizeof(msg)) == -1) {
            set_error("RPI_GPIO_PUD: %s", strerror(errno));
            return NULL;
        }
    }

    return PyLong_FromUnsignedLong(msg.value);
}

static PyObject *
rpi_gpio_output(PyObject *self, PyObject *args)
{
    unsigned    gpio;
    unsigned    value;

    if (!PyArg_ParseTuple(args, "II", &gpio, &value)) {
        return NULL;
    }

    gpio = get_gpio(gpio);
    if (gpio == INVAL_GPIO) {
        set_error("Invalid GPIO number");
        return NULL;
    }

    if ((value != 0) && (value != 1)) {
        set_error("Invalid GPIO output value");
        return NULL;
    }

    rpi_gpio_msg_t  msg = {
        .hdr.type = _IO_MSG,
        .hdr.subtype = RPI_GPIO_WRITE,
        .hdr.mgrid = RPI_GPIO_IOMGR,
        .gpio = gpio,
        .value = value
    };

    if (MsgSend(gpio_fd, &msg, sizeof(msg), NULL, 0) == -1) {
        set_error("RPI_GPIO_WRITE: %s", strerror(errno));
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *
rpi_gpio_input(PyObject *self, PyObject *args)
{
    unsigned    gpio;
    unsigned    value;

    if (!PyArg_ParseTuple(args, "I", &gpio, &value)) {
        return NULL;
    }

    gpio = get_gpio(gpio);
    if (gpio == INVAL_GPIO) {
        set_error("Invalid GPIO number");
        return NULL;
    }

    rpi_gpio_msg_t  msg = {
        .hdr.type = _IO_MSG,
        .hdr.subtype = RPI_GPIO_READ,
        .hdr.mgrid = RPI_GPIO_IOMGR,
        .gpio = gpio
    };

    if (MsgSend(gpio_fd, &msg, sizeof(msg), &msg, sizeof(msg)) == -1) {
        set_error("RPI_GPIO_READ: %s", strerror(errno));
        return NULL;
    }

    return PyLong_FromUnsignedLong(msg.value);
}

static PyObject *
rpi_gpio_setmode(PyObject *self, PyObject *args)
{
    unsigned    value;

    if (!PyArg_ParseTuple(args, "I", &value)) {
        return NULL;
    }

    if (value == 0) {
        gpio_table = board_gpios;
        gpio_table_max = 40;
    } else if (value == 1) {
        gpio_table = bcm_gpios;
        gpio_table_max = 27;
    } else {
        set_error("Unknown mode value");
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *
rpi_gpio_add_event(PyObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = {
        "gpio",
        "edge",
        "match",
        "callback",
        NULL
    };

    unsigned    gpio;
    unsigned    edge;
    unsigned    match = 1;
    PyObject    *func;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "II|IO", kwlist, &gpio,
                                     &edge, &match, &func)) {
        return NULL;
    }

    unsigned    gpio_num = gpio;
    gpio = get_gpio(gpio);
    if (gpio == INVAL_GPIO) {
        set_error("Invalid GPIO number");
        return NULL;
    }

    if (PyCallable_Check(func) == 0) {
        set_error("Function is not a callable object");
        return NULL;
    }

    if (event_coid == -1) {
        if (!init_event_thread()) {
            return NULL;
        }
    }

    callback_t  *cb = malloc(sizeof(callback_t));
    if (cb == NULL) {
        return NULL;
    }

    if (pthread_mutex_lock(&callback_table_lock) != 0) {
        return NULL;
    }

    Py_XINCREF(func);
    cb->func = func;
    cb->gpio_num = gpio_num;
    cb->next = callback_table[gpio];
    callback_table[gpio] = cb;

    pthread_mutex_unlock(&callback_table_lock);

    rpi_gpio_event_t  msg = {
        .hdr.type = _IO_MSG,
        .hdr.subtype = RPI_GPIO_ADD_EVENT,
        .hdr.mgrid = RPI_GPIO_IOMGR,
        .gpio = gpio,
        .detect = edge,
        .match = match
    };

    SIGEV_PULSE_INIT(&msg.event, event_coid, -1, _PULSE_CODE_MINAVAIL, gpio);
    if (MsgRegisterEvent(&msg.event, gpio_fd) == -1) {
        set_error("RPI_GPIO_ADD_EVENT: %s", strerror(errno));
        return NULL;
    }

    if (MsgSend(gpio_fd, &msg, sizeof(msg), NULL, 0) == -1) {
        set_error("RPI_GPIO_ADD_EVENT: %s", strerror(errno));
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *
rpi_gpio_init_spi(PyObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = {
        "clkdiv",
        NULL
    };

    unsigned    clkdiv;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "I", kwlist, &clkdiv)) {
        return NULL;
    }

    rpi_gpio_spi_t  msg = {
        .hdr.type = _IO_MSG,
        .hdr.subtype = RPI_GPIO_SPI_INIT,
        .hdr.mgrid = RPI_GPIO_IOMGR,
        .clkdiv = clkdiv
    };

    if (MsgSend(gpio_fd, &msg, sizeof(msg), NULL, 0) == -1) {
        set_error("RPI_GPIO_SPI_INIT: %s", strerror(errno));
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *
rpi_gpio_write_spi(PyObject *self, PyObject *args)
{
    struct {
        rpi_gpio_spi_t  msg;
        uint8_t         data[8];
    }   smsg = {
        .msg.hdr.type = _IO_MSG,
        .msg.hdr.subtype = RPI_GPIO_SPI_WRITE_READ,
        .msg.hdr.mgrid = RPI_GPIO_IOMGR
    };

    PyObject *dataList;
    if (!PyArg_ParseTuple(args, "O!", &PyList_Type, &dataList)) {
        return NULL;
    }

    size_t const    size = PyList_Size(dataList);
    if (size > 8) {
        set_error("List has too many members: %zu", size);
        return NULL;
    }

    for (size_t i = 0; i < size; i++) {
        PyObject * const    obj = PyList_GetItem(dataList, i);
        if (obj == NULL) {
            set_error("Bad list item: %zu", i);
            return NULL;
        }
        smsg.data[i] = PyLong_AsLong(obj);
    }

    if (MsgSend(gpio_fd, &smsg, sizeof(smsg.msg) + size, NULL, 0) == -1) {
        set_error("RPI_GPIO_SPI_WRITE_READ: %s", strerror(errno));
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *
rpi_gpio_write_read_spi(PyObject *self, PyObject *args)
{
    struct {
        rpi_gpio_spi_t  msg;
        uint8_t         data[8];
    }   smsg = {
        .msg.hdr.type = _IO_MSG,
        .msg.hdr.subtype = RPI_GPIO_SPI_WRITE_READ,
        .msg.hdr.mgrid = RPI_GPIO_IOMGR
    };

    unsigned    cs;
    PyObject   *inDataList;
    PyObject   *outDataList;
    if (!PyArg_ParseTuple(args, "IO!O!", &cs, &PyList_Type, &inDataList,
                          &PyList_Type, &outDataList)) {
        return NULL;
    }

    size_t const    inSize = PyList_Size(inDataList);
    if (inSize > 8) {
        set_error("List has too many members: %zu", inSize);
        return NULL;
    }

    size_t const    outSize = PyList_Size(outDataList);
    if (outSize > 8) {
        set_error("List has too many members: %zu", outSize);
        return NULL;
    }

    smsg.msg.cs = cs;
    for (size_t i = 0; i < inSize; i++) {
        PyObject * const    obj = PyList_GetItem(inDataList, i);
        if (obj == NULL) {
            set_error("Bad list item: %zu", i);
            return NULL;
        }
        smsg.data[i] = PyLong_AsLong(obj);
    }

    if (MsgSend(gpio_fd, &smsg, sizeof(smsg.msg) + inSize, &smsg,
                sizeof(smsg.msg) + outSize) == -1) {
        set_error("RPI_GPIO_SPI_WRITE_READ: %s", strerror(errno));
        return NULL;
    }

    for (size_t i = 0; i < outSize; i++) {
        PyObject * const    obj = PyLong_FromLong(smsg.data[i]);
        if (obj == NULL) {
            set_error("Failed to convert result");
            return NULL;
        }
        PyList_SetItem(outDataList, i, obj);
    }

    Py_RETURN_NONE;
}

static PyObject *
rpi_gpio_cleanup(PyObject *self, PyObject *args)
{
    close(gpio_fd);
    Py_RETURN_NONE;
}

static PyMethodDef rpi_gpio_methods[] = {
    {
        "setup",
        (PyCFunction)rpi_gpio_setup,
        METH_VARARGS | METH_KEYWORDS,
        "Configure a GPIO pin as input/output."
    },
    {
        "output",
        rpi_gpio_output,
        METH_VARARGS,
        "Turn a GPIO output on or off."
    },
    {
        "input",
        rpi_gpio_input,
        METH_VARARGS,
        "Read the status of a GPIO input."
    },
    {
        "setmode",
        rpi_gpio_setmode,
        METH_VARARGS,
        "Use BOARD/BCM pin layout."
    },
    {
        "add_event_detect",
        (PyCFunction)rpi_gpio_add_event,
        METH_VARARGS | METH_KEYWORDS,
        "Enable event detection for a GPIO input."
    },
    {
        "init_spi",
        (PyCFunction)rpi_gpio_init_spi,
        METH_VARARGS | METH_KEYWORDS,
        "Initialize the SPI interface."
    },
    {
        "write_spi",
        rpi_gpio_write_spi,
        METH_VARARGS,
        "Write bytes to the SPI interface."
    },
    {
        "write_read_spi",
        rpi_gpio_write_read_spi,
        METH_VARARGS,
        "Write bytes to and then read from the SPI interface."
    },
    {
        "cleanup",
        rpi_gpio_cleanup,
        METH_VARARGS,
        "Restore all used GPIOs to their original state."
    },
    { NULL, NULL, 0, NULL }
};

static struct PyModuleDef   moduledef = {
    PyModuleDef_HEAD_INIT,
    "rpi_gpio",
    "GPIO support for RaspberryPi (QNX)",
    -1,
    rpi_gpio_methods
};

PyMODINIT_FUNC
PyInit_rpi_gpio(void)
{
    // Establish a connection to the GPIO resource manager.
    gpio_fd = open("/dev/gpio/msg", O_RDWR);
    if (gpio_fd == -1) {
        return NULL;
    }

    // Default to the BCM PIN layout.
    gpio_table = bcm_gpios;
    gpio_table_max = 27;

    // Register the module.
    PyObject * const    module = PyModule_Create(&moduledef);
    if (module == NULL) {
        return NULL;
    }

    gpio_err = PyErr_NewException("gpio_rpi.error", NULL, NULL);
    Py_INCREF(gpio_err);
    PyModule_AddObject(module, "error", gpio_err);

    // Register constants.
    for (unsigned i = 0; i < sizeof(constants) / sizeof(constants[0]); i++) {
        constants[i].obj = Py_BuildValue("I", constants[i].value);
        PyModule_AddObject(module, constants[i].name, constants[i].obj);
    }

    extern PyTypeObject *PWM_init_PWMType(void);
    PyTypeObject    *pwm_type = PWM_init_PWMType();
    if (pwm_type == NULL) {
        return NULL;
    }

    Py_INCREF(pwm_type);
    PyModule_AddObject(module, "PWM", (PyObject*)pwm_type);
    return module;
}

////////////////////////////////////////////////////////////////////////////////
// Functions needed by the PWM module.
////////////////////////////////////////////////////////////////////////////////

int
get_gpio_number(int channel, unsigned *gpio)
{
    unsigned    gpio_num = get_gpio(channel);
    if (gpio_num == 0) {
        return -1;
    }

    *gpio = gpio_num;
    return 0;
}

void
pwm_set_frequency(unsigned const gpio, unsigned const frequency,
                  unsigned const range, unsigned const mode)
{
    rpi_gpio_pwm_t  msg = {
        .hdr.type = _IO_MSG,
        .hdr.subtype = RPI_GPIO_PWM_SETUP,
        .hdr.mgrid = RPI_GPIO_IOMGR,
        .gpio = gpio,
        .frequency = frequency,
        .range = range,
        .mode = mode
    };

    if (MsgSend(gpio_fd, &msg, sizeof(msg), NULL, 0) == -1) {
        set_error("RPI_GPIO_PWM_SETUP: %s", strerror(errno));
    }
}

void
pwm_set_duty_cycle(unsigned const gpio, unsigned const duty,
                   unsigned const range)
{
    rpi_gpio_msg_t  msg = {
        .hdr.type = _IO_MSG,
        .hdr.subtype = RPI_GPIO_PWM_DUTY,
        .hdr.mgrid = RPI_GPIO_IOMGR,
        .gpio = gpio,
        .value = duty
    };

    if (MsgSend(gpio_fd, &msg, sizeof(msg), NULL, 0) == -1) {
        set_error("RPI_GPIO_PWM_DUTY: %s", strerror(errno));
    }
}

void
pwm_start(unsigned gpio)
{
}

void
pwm_stop(unsigned gpio)
{
    pwm_set_duty_cycle(gpio, 0, 1);
}

/*
Copyright (c) 2013-2018 Ben Croston

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "Python.h"
#include "py_pwm.h"

typedef struct
{
    PyObject_HEAD
    unsigned    gpio;
    float       freq;
    unsigned    dutycycle;
    unsigned    range;
    unsigned    mode;
} PWMObject;

// python method PWM.__init__(self, channel, frequency)
static int PWM_init(PWMObject *self, PyObject *args, PyObject *kwargs)
{
    int         channel;
    float       frequency;
    unsigned    range = 1024;
    unsigned    mode = 1;

    static char *kwlist[] = {
        "channel",
        "frequency",
        "range",
        "mode",
        NULL
    };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "if|II", kwlist,
                                     &channel, &frequency, &range, &mode)) {
        return -1;
    }

    // convert channel to gpio
    if (get_gpio_number(channel, &(self->gpio))) {
        return -1;
    }

    if (frequency <= 0.0) {
        PyErr_SetString(PyExc_ValueError, "frequency must be greater than 0.0");
        return -1;
    }

    self->freq = frequency;
    self->range = range;
    self->mode = mode;

    pwm_set_frequency(self->gpio, self->freq, self->range, self->mode);
    return 0;
}

// python method PWM.start(self, dutycycle)
static PyObject *PWM_start(PWMObject *self, PyObject *args)
{
    float dutycycle;

    if (!PyArg_ParseTuple(args, "f", &dutycycle))
        return NULL;

    if (dutycycle < 0.0 || dutycycle > 100.0)
    {
        PyErr_SetString(PyExc_ValueError, "dutycycle must have a value from 0.0 to 100.0");
        return NULL;
    }

    self->dutycycle = (self->range * dutycycle / 100);
    pwm_set_duty_cycle(self->gpio, self->dutycycle, self->range);
    pwm_start(self->gpio);
    Py_RETURN_NONE;
}

// python method PWM.ChangeDutyCycle(self, dutycycle)
static PyObject *PWM_ChangeDutyCycle(PWMObject *self, PyObject *args)
{
    float dutycycle = 0.0;
    if (!PyArg_ParseTuple(args, "f", &dutycycle))
        return NULL;

    if (dutycycle < 0.0 || dutycycle > 100.0)
    {
        PyErr_SetString(PyExc_ValueError, "dutycycle must have a value from 0.0 to 100.0");
        return NULL;
    }

    self->dutycycle = (self->range * dutycycle / 100);
    pwm_set_duty_cycle(self->gpio, self->dutycycle, self->range);
    Py_RETURN_NONE;
}

// python method PWM.ChangeDutyCycleAbs(self, dutycycle)
static PyObject *PWM_ChangeDutyCycleAbs(PWMObject *self, PyObject *args)
{
    unsigned dutycycle = 0.0;
    if (!PyArg_ParseTuple(args, "I", &dutycycle))
        return NULL;

    if (dutycycle > self->range)
    {
        PyErr_SetString(PyExc_ValueError, "dutycycle must have a value from 0 to <range>");
        return NULL;
    }

    self->dutycycle = dutycycle;
    pwm_set_duty_cycle(self->gpio, self->dutycycle, self->range);
    Py_RETURN_NONE;
}

// python method PWM. ChangeFrequency(self, frequency)
static PyObject *PWM_ChangeFrequency(PWMObject *self, PyObject *args)
{
    float frequency = 1.0;

    if (!PyArg_ParseTuple(args, "f", &frequency))
        return NULL;

    if (frequency <= 0.0)
    {
        PyErr_SetString(PyExc_ValueError, "frequency must be greater than 0.0");
        return NULL;
    }

    self->freq = frequency;

    pwm_set_frequency(self->gpio, self->freq, self->range, self->mode);
    Py_RETURN_NONE;
}

// python function PWM.stop(self)
static PyObject *PWM_stop(PWMObject *self, PyObject *args)
{
    pwm_stop(self->gpio);
    Py_RETURN_NONE;
}

// deallocation method
static void PWM_dealloc(PWMObject *self)
{
    pwm_stop(self->gpio);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyMethodDef
PWM_methods[] = {
   { "start", (PyCFunction)PWM_start, METH_VARARGS, "Start software PWM\ndutycycle - the duty cycle (0.0 to 100.0)" },
   { "ChangeDutyCycle", (PyCFunction)PWM_ChangeDutyCycle, METH_VARARGS, "Change the duty cycle\ndutycycle - between 0.0 and 100.0" },
   { "ChangeDutyCycleAbs", (PyCFunction)PWM_ChangeDutyCycleAbs, METH_VARARGS, "Change the duty cycle\ndutycycle - between 0 and range" },
   { "ChangeFrequency", (PyCFunction)PWM_ChangeFrequency, METH_VARARGS, "Change the frequency\nfrequency - frequency in Hz (freq > 1.0)" },
   { "stop", (PyCFunction)PWM_stop, METH_VARARGS, "Stop software PWM" },
   { NULL }
};

PyTypeObject PWMType = {
   PyVarObject_HEAD_INIT(NULL,0)
   "rpi_gpio.PWM",            // tp_name
   sizeof(PWMObject),         // tp_basicsize
   0,                         // tp_itemsize
   (destructor)PWM_dealloc,   // tp_dealloc
   0,                         // tp_print
   0,                         // tp_getattr
   0,                         // tp_setattr
   0,                         // tp_compare
   0,                         // tp_repr
   0,                         // tp_as_number
   0,                         // tp_as_sequence
   0,                         // tp_as_mapping
   0,                         // tp_hash
   0,                         // tp_call
   0,                         // tp_str
   0,                         // tp_getattro
   0,                         // tp_setattro
   0,                         // tp_as_buffer
   Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, // tp_flag
   "Pulse Width Modulation class",    // tp_doc
   0,                         // tp_traverse
   0,                         // tp_clear
   0,                         // tp_richcompare
   0,                         // tp_weaklistoffset
   0,                         // tp_iter
   0,                         // tp_iternext
   PWM_methods,               // tp_methods
   0,                         // tp_members
   0,                         // tp_getset
   0,                         // tp_base
   0,                         // tp_dict
   0,                         // tp_descr_get
   0,                         // tp_descr_set
   0,                         // tp_dictoffset
   (initproc)PWM_init,        // tp_init
   0,                         // tp_alloc
   0,                         // tp_new
};

PyTypeObject *PWM_init_PWMType(void)
{
   // Fill in some slots in the type, and make it ready
   PWMType.tp_new = PyType_GenericNew;
   if (PyType_Ready(&PWMType) < 0) {
      return NULL;
   }

   PyDict_SetItemString(PWMType.tp_dict, "MODE_MS",
                        Py_BuildValue("I", 1));
   PyDict_SetItemString(PWMType.tp_dict, "MODE_PWM",
                        Py_BuildValue("I", 0));

   return &PWMType;
}

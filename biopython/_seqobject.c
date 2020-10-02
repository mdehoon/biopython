#define PY_SSIZE_T_CLEAN

#include "Python.h"
#include "structmember.h"


static int _get_buffer(PyObject* data, Py_buffer* view) {
    view->obj = NULL;
    if (PyObject_GetBuffer(data, view, PyBUF_STRIDES | PyBUF_FORMAT) < 0) {
        PyErr_Clear();
        if (PyUnicode_Check(data)) {
            if (PyUnicode_READY(data) == -1) return -1;
            if (PyUnicode_KIND(data) != PyUnicode_1BYTE_KIND) {
                PyErr_SetString(PyExc_TypeError, "string should be ASCII");
                return -1;
            }
            if (PyBuffer_FillInfo(view, data, PyUnicode_DATA(data),
                                  PyUnicode_GET_LENGTH(data), 1,
                                  PyBUF_STRIDES | PyBUF_FORMAT) < 0) return -1;
        }
        else {
            const char* type = Py_TYPE(data)->tp_name;
            if (PySequence_Check(data)) {
                data = PySequence_GetSlice(data, 0, PY_SSIZE_T_MAX);
                if (!data) return -1;
                if (!PyBytes_Check(data)) {
                    PyErr_SetString(PyExc_ValueError, "data should return bytes");
                    Py_DECREF(data);
                    return -1;
                }
            }
            else if ((data = PyObject_Bytes(data)) == NULL) {
                PyErr_Format(PyExc_TypeError,
                             "data of type %s do not provide the buffer "
                             "protocol or the sequence protocol", type);
                return -1;
            }
            if (PyObject_GetBuffer(data, view, PyBUF_STRIDES | PyBUF_FORMAT) < 0) {
                Py_DECREF(data);
                return -1;
            }
            Py_DECREF(data);
        }
    }
    if (view->ndim != 1 || view->strides[0] > 1
     || strcmp(view->format, "B") != 0) {
        PyErr_SetString(PyExc_ValueError, "unexpected buffer data");
        PyBuffer_Release(view);
        return -1;
    }
    return 0;
}


static PyTypeObject SeqType;

typedef struct {
    PyObject_HEAD
    PyObject* data;
    PyObject* id;
    PyObject* name;
    PyObject* description;
    PyObject* annotations;
    PyObject* features;
    PyObject* dbxrefs;
    PyObject* letter_annotations;
} SeqObject;

static SeqObject *
Seq_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    SeqObject *object;
    PyObject *data = Py_None;
    PyObject *id = NULL;
    PyObject *name = NULL;
    PyObject *description = NULL;
    PyObject *annotations = NULL;
    PyObject *features = NULL;
    PyObject *dbxrefs = NULL;
    PyObject *letter_annotations = NULL;

    char* character = NULL;

    static char *kwlist[] = {"data", "id", "name", "description", "annotations",
                             "features", "dbxrefs", "letter_annotations",
                             "character", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|OOOOOOOOs", kwlist,
                                     &data, &id, &name, &description,
                                     &annotations, &features, &dbxrefs,
                                     &letter_annotations, &character))
        return NULL;

    if (id) {
        if (PyUnicode_Check(id)) {
            if (PyUnicode_CompareWithASCIIString(id, "") == 0) id = NULL;
        }
        else if (id != Py_None) {
            PyErr_Format(PyExc_TypeError,
                "attribute id requires a string or None (received type %s)",
                Py_TYPE(id)->tp_name);
            return NULL;
        }
    }

    if (name) {
        if (PyUnicode_Check(name)) {
            if (PyUnicode_CompareWithASCIIString(name, "") == 0) name = NULL;
        }
        else if (name != Py_None) {
            PyErr_Format(PyExc_TypeError,
                "attribute name requires a string or None (received type %s)",
                Py_TYPE(name)->tp_name);
            return NULL;
        }
    }

    if (description) {
        if (PyUnicode_Check(description)) {
            if (PyUnicode_CompareWithASCIIString(description, "") == 0)
                description = NULL;
        }
        else if (description != Py_None) {
            PyErr_Format(PyExc_TypeError,
                "attribute description requires a string or None (received type %s)",
                Py_TYPE(description)->tp_name);
            return NULL;
        }
    }

    if (annotations == Py_None) annotations = NULL;
    else if (annotations) {
        if (!PyDict_Check(annotations)) {
            PyErr_Format(PyExc_TypeError,
                "attribute annotations requires a dictionary (received type %s)",
                Py_TYPE(annotations)->tp_name);
            return NULL;
        }
    }

    if (features == Py_None) features = NULL;
    else if (features) {
        if (!PyList_Check(features)) {
            PyErr_Format(PyExc_TypeError,
                "attribute features requires a list (received type %s)",
                Py_TYPE(features)->tp_name);
            return NULL;
        }
    }

    if (dbxrefs == Py_None) dbxrefs = NULL;
    else if (dbxrefs) {
        if (!PyList_Check(dbxrefs)) {
            PyErr_Format(PyExc_TypeError,
                "attribute dbxrefs requires a list (received type %s)",
                Py_TYPE(dbxrefs)->tp_name);
            return NULL;
        }
    }

    if (letter_annotations == Py_None) letter_annotations = NULL;
    else if (letter_annotations) {
        if (!PyDict_Check(letter_annotations)) {
            PyErr_Format(PyExc_TypeError,
                "attribute letter_annotations requires a dictionary (received type %s)",
                Py_TYPE(letter_annotations)->tp_name);
            return NULL;
        }
    }

    if (PyLong_Check(data)) {
        Py_buffer *buffer;
        PyObject* bytes;
        Py_ssize_t length = PyLong_AsSsize_t(data);
        if (length == -1 && PyErr_Occurred()) return NULL;
        if (length < 0) {
            PyErr_Format(PyExc_ValueError,
                         "expected sequence data or a positive integer "
                         "(received %zd)", length);
            return NULL;
        }
        if (character) {
            if (strlen(character) != 1) {
                PyErr_SetString(PyExc_ValueError,
                    "character should be a single letter");
                return NULL;
            }
            bytes = PyBytes_FromStringAndSize(character, 1);
        }
        else
            bytes = PyBytes_FromStringAndSize("?", 1);
        if (!bytes) return NULL;
        data = PyMemoryView_FromObject(bytes);
        Py_DECREF(bytes);
        if (!data) return NULL;
        buffer = PyMemoryView_GET_BUFFER(data);
        buffer->len = length;
        buffer->shape[0] = buffer->len;
        buffer->strides[0] = 0;
    }
    else {
        if (character != NULL) {
            PyErr_SetString(PyExc_ValueError,
                "character should be None if data is given");
            return NULL;
        }
        if (PyUnicode_Check(data)) {
            data = PyUnicode_AsASCIIString(data);
            if (!data) return NULL;
        }
        else if (PyObject_IsInstance(data, (PyObject*)&SeqType)) {
            data = ((SeqObject*)data)->data;
            if (PyByteArray_Check(data)) {
                /* make a copy for mutable data */
                data = PyByteArray_FromObject(data);
                if (!data) return NULL;
            }
            else Py_INCREF(data);
        }
        else {
            Py_INCREF(data);
        }
    }

    object = (SeqObject*)type->tp_alloc(type, 0);
    if (!object) {
        Py_DECREF(data);
        return NULL;
    }

    Py_XINCREF(id);
    Py_XINCREF(name);
    Py_XINCREF(description);
    Py_XINCREF(annotations);
    Py_XINCREF(features);
    Py_XINCREF(dbxrefs);
    Py_XINCREF(letter_annotations);

    object->data = data;
    object->id = id;
    object->name = name;
    object->description = description;
    object->annotations = annotations;
    object->features = features;
    object->dbxrefs = dbxrefs;
    object->letter_annotations = letter_annotations;

    return object;
}

static void
Seq_dealloc(SeqObject *op)
{
    Py_DECREF(op->data);
    Py_XDECREF(op->id);
    Py_XDECREF(op->description);
    Py_XDECREF(op->annotations);
    Py_XDECREF(op->features);
    Py_XDECREF(op->dbxrefs);
    Py_XDECREF(op->letter_annotations);
    Py_TYPE(op)->tp_free(op);
}

static PyObject*
Seq_repr(SeqObject* self)
{
    PyObject* result = NULL;
    PyObject* sequence;
    PyObject* data = self->data;
    Py_buffer view;
    Py_ssize_t n;
    Py_ssize_t i;
    PyObject* list;
    if (PyObject_GetBuffer(data, &view, PyBUF_STRIDES | PyBUF_FORMAT) == 0) {
        const char* s;
        if (view.ndim != 1 || strcmp(view.format, "B") != 0) {
            PyErr_SetString(PyExc_ValueError, "unexpected buffer data");
            PyBuffer_Release(&view);
            return NULL;
        }
        n = view.len;
        s = view.buf;
        if (view.strides[0] == 0) {
            sequence = PyUnicode_FromFormat("%d, character='%c'", n, s[0]);
        }
        else if (view.strides[0] != 1) {
            PyErr_SetString(PyExc_ValueError, "unexpected buffer data");
            sequence = NULL;
        }
        else if (n <= 60) {
            sequence = PyUnicode_New(n+2, 127);
            if (sequence) {
                char* t = PyUnicode_DATA(sequence);
                t[0] = '\'';
                memcpy(t+1, s, n);
                t[n+1] = '\'';
                t[n+2] = '\0';
            }
        }
        else
            sequence = PyUnicode_FromFormat("'%.54s...%.3s'", s, s + n - 3);
        PyBuffer_Release(&view);
    }
    else if (PySequence_Check(data)) {
        n = PySequence_Length(data);
        if (n <= 60) {
            PyObject* slice = PySequence_GetSlice(data, 0, n);
            if (!slice || !PyBytes_Check(slice)) {
                Py_XDECREF(slice);
                PyErr_SetString(PyExc_ValueError, "data should return bytes");
                return NULL;
            }
            sequence = PyUnicode_New(n+2, 127);
            if (sequence) {
                char* t = PyUnicode_DATA(sequence);
                t[0] = '\'';
                memcpy(t+1, PyBytes_AS_STRING(slice), n);
                t[n+1] = '\'';
                t[n+2] = '\0';
            }
            Py_DECREF(slice);
        }
        else {
            PyObject* slice1 = PySequence_GetSlice(data, 0, 54);
            PyObject* slice2 = PySequence_GetSlice(data, n - 3, n);
            if (!slice1 || !PyBytes_Check(slice1)
             || !slice2 || !PyBytes_Check(slice2)) {
                Py_XDECREF(slice1);
                Py_XDECREF(slice2);
                PyErr_SetString(PyExc_ValueError, "data should return bytes");
                return NULL;
            }
            sequence = PyUnicode_FromFormat("'%s...%s'",
                                            PyBytes_AS_STRING(slice1),
                                            PyBytes_AS_STRING(slice2));
            Py_DECREF(slice1);
            Py_DECREF(slice2);
        }
    }
    else {
        PyErr_SetString(PyExc_ValueError,
            "data should support the buffer protocol or the sequence protocol");
        return NULL;
    }
    if (sequence) i = 1; else return NULL;
    if (self->id) i++;
    if (self->name) i++;
    if (self->description) i++;
    list = PyList_New(i);
    if (!list) {
        Py_DECREF(sequence);
        return NULL;
    }
    i = 0;
    PyList_SET_ITEM(list, i++, sequence);
    if (self->id) {
        PyObject* argument = PyUnicode_FromFormat("id='%U'", self->id);
        if (!argument) goto exit;
        PyList_SET_ITEM(list, i++, argument);
    }
    if (self->name) {
        PyObject* argument = PyUnicode_FromFormat("name='%U'", self->name);
        if (!argument) goto exit;
        PyList_SET_ITEM(list, i++, argument);
    }
    if (self->description) {
        PyObject* argument = PyUnicode_FromFormat("description='%U'", self->description);
        if (!argument) goto exit;
        PyList_SET_ITEM(list, i++, argument);
    }
    if (i == 1) {
        result = PyUnicode_FromFormat("%s(%U)", Py_TYPE(self)->tp_name, sequence);
    }
    else {
        PyObject* arguments;
        PyObject* separator = PyUnicode_FromStringAndSize(", ", 2);
        if (!separator) goto exit;
        arguments = PyUnicode_Join(separator, list);
        Py_DECREF(separator);
        if (!arguments) goto exit;
        result = PyUnicode_FromFormat("%s(%U)", Py_TYPE(self)->tp_name, arguments);
        Py_DECREF(arguments);
    }
exit:
    Py_DECREF(list);
    return result;
}

static PyObject*
Seq_str(SeqObject* self)
{
    Py_buffer view;
    PyObject *string;
    if (_get_buffer(self->data, &view) < 0) return NULL;
    string = PyUnicode_New(view.len, 127);
    if (string) {
        char *s = PyUnicode_DATA(string);
        const char* t = view.buf;
        if (view.strides[0] == 0)
            memset(s, t[0], view.len);
        else
            memcpy(s, t, view.len);
    }
    PyBuffer_Release(&view);
    return string;
}

static PyObject *
Seq_number_add(PyObject* seq1, PyObject* seq2)
{
    char* s1;
    char* s2;
    Py_ssize_t length1;
    Py_ssize_t length2;
    SeqObject* object = NULL;
    PyObject* data;
    PyTypeObject* type = NULL;
    Py_buffer view1;
    Py_buffer view2;

    view1.obj = NULL;
    view2.obj = NULL;

    if (PyObject_IsInstance(seq1, (PyObject*)&SeqType)) {
        if (_get_buffer(((SeqObject*)seq1)->data, &view1) < 0) goto exit;
        type = Py_TYPE(seq1);
    }
    else
        if (_get_buffer(seq1, &view1) < 0) goto exit;

    if (PyObject_IsInstance(seq2, (PyObject*)&SeqType)) {
        if (_get_buffer(((SeqObject*)seq2)->data, &view2) < 0) goto exit;
        if (!type) type = Py_TYPE(seq2);
    }
    else
        if (_get_buffer(seq2, &view2) < 0) goto exit;

    length1 = view1.len;
    length2 = view2.len;
    s1 = view1.buf;
    s2 = view2.buf;

    if (view1.strides[0] == 0 && view2.strides[0] == 0 && s1[0] == s2[0]) {
        Py_buffer *buffer;
        PyObject* object = PyBytes_FromStringAndSize(s1, 1);
        if (!object) goto exit;
        data = PyMemoryView_FromObject(object);
        Py_DECREF(object);
        if (!data) goto exit;
        buffer = PyMemoryView_GET_BUFFER(data);
        buffer->len = length1 + length2;
        buffer->shape[0] = buffer->len;
        buffer->strides[0] = 0;
    }
    else {
        char* s;
        data = PyBytes_FromStringAndSize(NULL, length1 + length2);
        if (!data) goto exit;
        s = PyBytes_AS_STRING(data);
        if (view1.strides && view1.strides[0] == 0)
            memset(s, ((char*)view1.buf)[0], length1);
        else
            memcpy(s, view1.buf, length1);
        if (view2.strides && view2.strides[0] == 0)
            memset(s + length1, ((char*)view2.buf)[0], length2);
        else
            memcpy(s + length1, view2.buf, length2);
        if (strcmp(type->tp_name, "UnknownSeq") == 0) { // FIXME
            type = type->tp_base;
        }
        if (strcmp(type->tp_name, "DBSeq") == 0) { // FIXME
            type = type->tp_base;
        }
    }

    object = (SeqObject*)PyType_GenericAlloc(type, 0);
    if (object) object->data = data;
    else Py_DECREF(data);

exit:
    PyBuffer_Release(&view1);
    PyBuffer_Release(&view2);

    return (PyObject*)object;
}

static PyNumberMethods Seq_as_number = {
    (binaryfunc)Seq_number_add,
    NULL,
};

static Py_ssize_t
Seq_mapping_length(SeqObject* self)
{
    return PyObject_Size(self->data);
}

static PyObject*
Seq_subscript(SeqObject *self, PyObject *key)
{
    PyObject* data = self->data;
    PyObject* result = NULL;
    Py_buffer view;
    const char* s;

    if (PyObject_GetBuffer(data, &view, PyBUF_STRIDES | PyBUF_FORMAT) < 0) {
        return PyObject_GetItem(data, key);
    }

    s = view.buf;

    if (PyIndex_Check(key)) {
        Py_ssize_t i = PyNumber_AsSsize_t(key, PyExc_IndexError);
        if (!(i == -1 && PyErr_Occurred())) {
            if (i < 0) i += view.len;
            if (i < 0 || i >= view.len)
                PyErr_SetString(PyExc_IndexError, "index out of range");
            else
                result = PyBytes_FromStringAndSize(s + i, 1);
        }
    }
    else if (PySlice_Check(key)) {
        Py_ssize_t start, stop, step, slicelength, cur, i;

        if (PySlice_Unpack(key, &start, &stop, &step) < 0) return NULL;

        slicelength = PySlice_AdjustIndices(view.len, &start, &stop, step);
        if (slicelength <= 0)
            result = PyBytes_FromStringAndSize("", 0);
        else if (step == 1)
            result = PyBytes_FromStringAndSize(s + start, slicelength);
        else {
            result = PyBytes_FromStringAndSize(NULL, slicelength);
            if (result) {
                char* target = PyBytes_AS_STRING(result);
                for (cur = start, i = 0; i < slicelength; cur += step, i++) {
                    target[i] = s[cur];
                }
            }
        }
    }
    else {
        PyErr_Format(PyExc_TypeError,
                     "Seq indices must be integers or slices, not %.200s",
                     Py_TYPE(key)->tp_name);
    }

    PyBuffer_Release(&view);

    return result;
}

static int
Seq_ass_subscript(SeqObject *self, PyObject *key, PyObject *value)
{
    int result;
    PyObject* data = self->data;

    if (!PyByteArray_Check(data)) {
        PyErr_SetString(PyExc_ValueError, "sequence is immutable");
        return -1;
    }

    if (!value) return PyObject_DelItem(data, key);

    if (PyUnicode_Check(value)) {
        value = PyUnicode_AsASCIIString(value);
        if (!value) return -1;
    }
    else Py_INCREF(value);

    if (PyIndex_Check(key)) {
        Py_buffer view;
        char* s;
        if (PyObject_GetBuffer(value, &view, PyBUF_STRIDES | PyBUF_FORMAT) < 0) {
            Py_DECREF(value);
            return -1;
        }
        Py_DECREF(value);
        if (view.ndim != 1 || view.len != 1 || strcmp(view.format, "B") != 0) {
            PyBuffer_Release(&view);
            PyErr_SetString(PyExc_RuntimeError, "expected a single byte");
            return -1;
        }
        s = view.buf;
        value = PyLong_FromLong((long)(*s));
        PyBuffer_Release(&view);
        if (!value) return -1;
    }

    result = PyObject_SetItem(data, key, value);
    Py_DECREF(value);
    return result;
}

static PyMappingMethods Seq_as_mapping = {
    (lenfunc)Seq_mapping_length,
    (binaryfunc)Seq_subscript,
    (objobjargproc)Seq_ass_subscript,
};

static int
Seq_bf_getbuffer(SeqObject *self, Py_buffer *view, int flags)
{
    int result;
    PyObject* data = self->data;
    if (!PyObject_CheckBuffer(data)) {
        data = PyObject_Bytes(data);
        if (!data) {
            PyErr_Format(PyExc_TypeError,
                         "data of type %s do not provide the buffer "
                         "protocol or the sequence protocol",
                         Py_TYPE(self->data)->tp_name);
            return -1;
        }
    }
    result = PyObject_GetBuffer(data, view, flags);
    if (data != self->data) Py_DECREF(data);
    return result;
}

static PyBufferProcs Seq_as_buffer = {
    (getbufferproc)Seq_bf_getbuffer,
    NULL,
};

static PyObject* compare_buffers(Py_buffer *view1, Py_buffer *view2, int op)
{
    int comparison;
    const char* s1 = view1->buf;
    const char* s2 = view2->buf;
    const Py_ssize_t n1 = view1->len;
    const Py_ssize_t n2 = view2->len;
    Py_ssize_t n;
    if (view1->strides && view1->strides[0] == 0 && view2->strides && view2->strides[0] == 0) {
        switch (op) {
        case Py_EQ:
            if (n1 != n2) Py_RETURN_FALSE;
            if (*s1 != *s2) Py_RETURN_FALSE;
            Py_RETURN_TRUE;
        case Py_NE:
            if (n1 != n2) Py_RETURN_TRUE;
            if (*s1 != *s2) Py_RETURN_TRUE;
            Py_RETURN_FALSE;
        case Py_LT:
            if (n2 == 0) Py_RETURN_FALSE;
            if (n1 == 0) Py_RETURN_TRUE;
            if (*s1 < *s2) Py_RETURN_TRUE;
            if (*s1 > *s2) Py_RETURN_FALSE;
            if (n1 < n2) Py_RETURN_TRUE;
            Py_RETURN_FALSE;
        case Py_LE:
            if (n1 == 0) Py_RETURN_TRUE;
            if (n2 == 0) Py_RETURN_FALSE;
            if (*s1 < *s2) Py_RETURN_TRUE;
            if (*s1 > *s2) Py_RETURN_FALSE;
            if (n1 <= n2) Py_RETURN_TRUE;
            Py_RETURN_FALSE;
        case Py_GT:
            if (n1 == 0) Py_RETURN_FALSE;
            if (n2 == 0) Py_RETURN_TRUE;
            if (*s1 < *s2) Py_RETURN_FALSE;
            if (*s1 > *s2) Py_RETURN_TRUE;
            if (n1 > n2) Py_RETURN_TRUE;
            Py_RETURN_FALSE;
        case Py_GE:
            if (n2 == 0) Py_RETURN_TRUE;
            if (n1 == 0) Py_RETURN_FALSE;
            if (*s1 > *s2) Py_RETURN_TRUE;
            if (*s1 < *s2) Py_RETURN_FALSE;
            if (n1 >= n2) Py_RETURN_TRUE;
            Py_RETURN_FALSE;
        }
    }
    n = (n1 < n2) ? n1 : n2;
    if (view1->strides && view1->strides[0] == 0) {
        Py_ssize_t i;
        const char c1 = *s1;
        switch (op) {
        case Py_EQ:
            if (n1 != n2) Py_RETURN_FALSE;
            for (i = 0; i < n; i++) if (c1 != s2[i]) Py_RETURN_FALSE;
            Py_RETURN_TRUE;
        case Py_NE:
            if (n1 != n2) Py_RETURN_TRUE;
            for (i = 0; i < n; i++) if (c1 != s2[i]) Py_RETURN_TRUE;
            Py_RETURN_FALSE;
        case Py_LT:
            if (n2 == 0) Py_RETURN_FALSE;
            if (n1 == 0) Py_RETURN_TRUE;
            for (i = 0; i < n; i++) {
                if (c1 < s2[i]) Py_RETURN_TRUE;
                if (c1 > s2[i]) Py_RETURN_FALSE;
            }
            if (n1 < n2) Py_RETURN_TRUE;
            Py_RETURN_FALSE;
        case Py_LE:
            if (n1 == 0) Py_RETURN_TRUE;
            if (n2 == 0) Py_RETURN_FALSE;
            for (i = 0; i < n; i++) {
                if (c1 < s2[i]) Py_RETURN_TRUE;
                if (c1 > s2[i]) Py_RETURN_FALSE;
            }
            if (n1 <= n2) Py_RETURN_TRUE;
            Py_RETURN_FALSE;
        case Py_GT:
            if (n1 == 0) Py_RETURN_FALSE;
            if (n2 == 0) Py_RETURN_TRUE;
            for (i = 0; i < n; i++) {
                if (c1 < s2[i]) Py_RETURN_FALSE;
                if (c1 > s2[i]) Py_RETURN_TRUE;
            }
            if (n1 <= n2) Py_RETURN_FALSE;
            Py_RETURN_TRUE;
        case Py_GE:
            if (n2 == 0) Py_RETURN_TRUE;
            if (n1 == 0) Py_RETURN_FALSE;
            for (i = 0; i < n; i++) {
                if (c1 < s2[i]) Py_RETURN_FALSE;
                if (c1 > s2[i]) Py_RETURN_TRUE;
            }
            if (n1 < n2) Py_RETURN_FALSE;
            Py_RETURN_TRUE;
        }
    }
    if (view2->strides && view2->strides[0] == 0) {
        Py_ssize_t i;
        const char c2 = *s2;
        switch (op) {
        case Py_EQ:
            if (n1 != n2) Py_RETURN_FALSE;
            for (i = 0; i < n; i++) if (s1[i] != c2) Py_RETURN_FALSE;
            Py_RETURN_TRUE;
        case Py_NE:
            if (n1 != n2) Py_RETURN_TRUE;
            for (i = 0; i < n; i++) if (s1[i] != c2) Py_RETURN_TRUE;
            Py_RETURN_FALSE;
        case Py_LT:
            if (n2 == 0) Py_RETURN_FALSE;
            if (n1 == 0) Py_RETURN_TRUE;
            for (i = 0; i < n; i++) {
                if (s1[i] < c2) Py_RETURN_TRUE;
                if (s1[i] > c2) Py_RETURN_FALSE;
            }
            if (n1 < n2) Py_RETURN_TRUE;
            Py_RETURN_FALSE;
        case Py_LE:
            if (n1 == 0) Py_RETURN_TRUE;
            if (n2 == 0) Py_RETURN_FALSE;
            for (i = 0; i < n; i++) {
                if (s1[i] < c2) Py_RETURN_TRUE;
                if (s1[i] > c2) Py_RETURN_FALSE;
            }
            if (n1 <= n2) Py_RETURN_TRUE;
            Py_RETURN_FALSE;
        case Py_GT:
            if (n1 == 0) Py_RETURN_FALSE;
            if (n2 == 0) Py_RETURN_TRUE;
            for (i = 0; i < n; i++) {
                if (s1[i] < c2) Py_RETURN_FALSE;
                if (s1[i] > c2) Py_RETURN_TRUE;
            }
            if (n1 <= n2) Py_RETURN_FALSE;
            Py_RETURN_TRUE;
        case Py_GE:
            if (n2 == 0) Py_RETURN_TRUE;
            if (n1 == 0) Py_RETURN_FALSE;
            for (i = 0; i < n; i++) {
                if (s1[i] < c2) Py_RETURN_FALSE;
                if (s1[i] > c2) Py_RETURN_TRUE;
            }
            if (n1 < n2) Py_RETURN_FALSE;
            Py_RETURN_TRUE;
        }
    }
    if ( (view1->strides && view1->strides[0] != 1)
      || (view2->strides && view2->strides[0] != 1) ) {
        PyErr_SetString(PyExc_ValueError, "data are not contiguous");
        return NULL;
    }
    comparison = memcmp(s1, s2, n);
    if (comparison == 0) {
        if (n1 < n2) comparison = -1;
        else if (n1 > n2) comparison = +1;
    }
    switch (op) {
    case Py_EQ:
        if (comparison == 0) Py_RETURN_TRUE;
        Py_RETURN_FALSE;
    case Py_NE:
        if (comparison == 0) Py_RETURN_FALSE;
        Py_RETURN_TRUE;
    case Py_LE:
        if (comparison <= 0) Py_RETURN_TRUE;
        Py_RETURN_FALSE;
    case Py_GE:
        if (comparison >= 0) Py_RETURN_TRUE;
        Py_RETURN_FALSE;
    case Py_LT:
        if (comparison < 0) Py_RETURN_TRUE;
        Py_RETURN_FALSE;
    case Py_GT:
        if (comparison > 0) Py_RETURN_TRUE;
        Py_RETURN_FALSE;
    default:
        PyErr_BadArgument();
        return NULL;
    }
}

static PyObject*
Seq_richcompare(SeqObject *self, PyObject *other, int op)
{
    Py_buffer view1;
    Py_buffer view2;
    PyObject *result;
    if ((PyObject*)self == other) {
        switch (op) {
        case Py_EQ:
        case Py_LE:
        case Py_GE:
            Py_RETURN_TRUE;
        case Py_NE:
        case Py_LT:
        case Py_GT:
            Py_RETURN_FALSE;
        default:
            PyErr_BadArgument();
            return NULL;
        }
    }

    if ( _get_buffer(self->data, &view1) < 0)
        return NULL;
    if ( _get_buffer(other, &view2) < 0) {
        PyBuffer_Release(&view1);
        return NULL;
    }
    if (view1.ndim != 1
     || view2.ndim != 1
     || strcmp(view1.format, "B") != 0
     || strcmp(view2.format, "B") != 0) {
        PyBuffer_Release(&view1);
        PyBuffer_Release(&view2);
        PyErr_Format(PyExc_TypeError,
                     "comparison not supported between instances of %s and %s",
                     Py_TYPE(self)->tp_name, Py_TYPE(other)->tp_name);
        return NULL;
    }

    result = compare_buffers(&view1, &view2, op);
    PyBuffer_Release(&view1);
    PyBuffer_Release(&view2);

    return result;
}

PyDoc_STRVAR(Seq_reduce_doc,
"__reduce__($self, /)\n"
"--\n"
"\n"
"Return state information for pickling.");

static PyObject *
Seq_reduce(SeqObject *self, PyObject *Py_UNUSED(ignored))
{
    PyObject* object;
    PyObject* id = self->id ? self->id : PyUnicode_New(0, 127);
    PyObject* name = self->name ? self->name : PyUnicode_New(0, 127);
    PyObject* description = self->description ? self->description : PyUnicode_New(0, 127);
    PyObject* annotations = self->annotations ? self->annotations : Py_None;
    PyObject* features = self->features ? self->features : Py_None;
    PyObject* dbxrefs = self->dbxrefs ? self->dbxrefs : Py_None;
    PyObject* letter_annotations = self->letter_annotations ? self->letter_annotations : Py_None;

    if (PyMemoryView_Check(self->data)) {
        const Py_buffer *buffer = PyMemoryView_GET_BUFFER(self->data);
        const Py_ssize_t length = buffer->len;
        const char* character;
        if (buffer->buf == NULL
           || buffer->obj == NULL || PyBytes_Check(buffer->obj) != 1
           || buffer->itemsize != 1
           || buffer->format == NULL || strcmp(buffer->format, "B") != 0
           || buffer->ndim != 1 || buffer->shape[0] != length
           || buffer->strides == NULL || buffer->strides[0] != 0
           || buffer->suboffsets != NULL) {
            PyErr_SetString(PyExc_ValueError, "data has unexpected buffer");
            return NULL;
        }
        character = buffer->buf;
        object = Py_BuildValue("O(nOOOOOOOs#)",
                               Py_TYPE(self), length, id, name,
                               description, annotations, features, dbxrefs,
                               letter_annotations, character, 1);
    }
    else
        object = Py_BuildValue("O(OOOOOOOO)",
                               Py_TYPE(self), self->data, id, name,
                               description, annotations, features, dbxrefs,
                               letter_annotations);

    if (!self->id) Py_DECREF(id);
    if (!self->name) Py_DECREF(name);
    if (!self->description) Py_DECREF(description);

    return object;
}

PyDoc_STRVAR(Seq_reverse_doc,
"Modify the mutable sequence to reverse itself.\n"
"\n"
"No return value.\n");

static PyObject *
Seq_reverse(SeqObject *self, PyObject *Py_UNUSED(ignored))
{
    Py_ssize_t i, n, m;
    char c;
    char* s;
    PyObject* data = self->data;

    if (!PyByteArray_Check(data)) {
        PyErr_SetString(PyExc_ValueError, "sequence is immutable");
        return NULL;
    }

    s = PyByteArray_AS_STRING(data);
    n = PyByteArray_GET_SIZE(data);
    m = n / 2;

    for (i = 0; i < m; i++) {
        c = s[i];
        s[i] = s[n-i-1];
        s[n-i-1] = c;
    }

    Py_RETURN_NONE;
}

PyDoc_STRVAR(Seq_complement_doc,
"Modify the mutable sequence into its RNA complement.\n"
"\n"
"No return value.\n");


static PyObject *
Seq_complement(SeqObject *self, PyObject *Py_UNUSED(ignored))
{
    Py_ssize_t i, n;
    char* s;
    PyObject* data = self->data;

    if (PyObject_CheckBuffer(data)) {
        Py_buffer view;
        if (PyObject_GetBuffer(data, &view, PyBUF_STRIDES | PyBUF_FORMAT) == 0) {
            const Py_ssize_t stride = view.strides[0];
            PyBuffer_Release(&view);
            if (stride == 0) {
                Py_INCREF(self);
                return (PyObject*)self;
            }
        }
        else PyErr_Clear();
    }

    if (PyBytes_Check(data)) {
        /* make sure we create a new bytes object;
         * note that the bytes object may have been interned */
        n = PyBytes_GET_SIZE(data);
        s = PyBytes_AS_STRING(data);
        data = PyBytes_FromStringAndSize(NULL, n);
        if (!data) return NULL;
        memcpy(PyBytes_AS_STRING(data), s, n);
        s = PyBytes_AS_STRING(data);
    }
    else if (PyByteArray_Check(data)) {
        n = PyByteArray_GET_SIZE(data);
        s = PyByteArray_AS_STRING(data);
    }
    else {
        data = PyBytes_FromObject(data);
        if (!data) return NULL;
        n = PyBytes_GET_SIZE(data);
        s = PyBytes_AS_STRING(data);
    }

    for (i = 0; i < n; i++) {
        switch (s[i]) {
            case 'A': s[i] = 'T'; break;
            case 'B': s[i] = 'V'; break;
            case 'C': s[i] = 'G'; break;
            case 'D': s[i] = 'H'; break;
            case 'G': s[i] = 'C'; break;
            case 'H': s[i] = 'D'; break;
            case 'K': s[i] = 'M'; break;
            case 'M': s[i] = 'K'; break;
            case 'N': s[i] = 'N'; break;
            case 'R': s[i] = 'Y'; break;
            case 'S': s[i] = 'S'; break;
            case 'T': s[i] = 'A'; break;
            case 'U': s[i] = 'A'; break;
            case 'V': s[i] = 'B'; break;
            case 'W': s[i] = 'W'; break;
            case 'X': s[i] = 'X'; break;
            case 'Y': s[i] = 'R'; break;
            case 'a': s[i] = 't'; break;
            case 'b': s[i] = 'v'; break;
            case 'c': s[i] = 'g'; break;
            case 'd': s[i] = 'h'; break;
            case 'g': s[i] = 'c'; break;
            case 'h': s[i] = 'd'; break;
            case 'k': s[i] = 'm'; break;
            case 'm': s[i] = 'k'; break;
            case 'n': s[i] = 'n'; break;
            case 'r': s[i] = 'y'; break;
            case 's': s[i] = 's'; break;
            case 't': s[i] = 'a'; break;
            case 'u': s[i] = 'a'; break;
            case 'v': s[i] = 'b'; break;
            case 'w': s[i] = 'w'; break;
            case 'x': s[i] = 'x'; break;
            case 'y': s[i] = 'r'; break;
            default: break;
        }
    }

    if (PyByteArray_Check(data)) Py_RETURN_NONE;

    return data;
}

PyDoc_STRVAR(Seq_rna_complement_doc,
"Modify the mutable sequence into its RNA complement.\n"
"\n"
"No return value.\n");

static PyObject *
Seq_rna_complement(SeqObject *self, PyObject *Py_UNUSED(ignored))
{
    Py_ssize_t i, n;
    char* s;
    PyObject* data = self->data;

    if (PyObject_CheckBuffer(data)) {
        Py_buffer view;
        if (PyObject_GetBuffer(data, &view, PyBUF_STRIDES | PyBUF_FORMAT) == 0) {
            const Py_ssize_t stride = view.strides[0];
            PyBuffer_Release(&view);
            if (stride == 0) {
                Py_INCREF(self);
                return (PyObject*)self;
            }
        }
        else PyErr_Clear();
    }

    if (PyBytes_Check(data)) {
        /* make sure we create a new bytes object;
         * note that the bytes object may have been interned */
        n = PyBytes_GET_SIZE(data);
        s = PyBytes_AS_STRING(data);
        data = PyBytes_FromStringAndSize(NULL, n);
        if (!data) return NULL;
        memcpy(PyBytes_AS_STRING(data), s, n);
        s = PyBytes_AS_STRING(data);
    }
    else if (PyByteArray_Check(data)) {
        n = PyByteArray_GET_SIZE(data);
        s = PyByteArray_AS_STRING(data);
    }
    else {
        data = PyBytes_FromObject(data);
        if (!data) return NULL;
        n = PyBytes_GET_SIZE(data);
        s = PyBytes_AS_STRING(data);
    }

    for (i = 0; i < n; i++) {
        switch (s[i]) {
            case 'A': s[i] = 'U'; break;
            case 'B': s[i] = 'V'; break;
            case 'C': s[i] = 'G'; break;
            case 'D': s[i] = 'H'; break;
            case 'G': s[i] = 'C'; break;
            case 'H': s[i] = 'D'; break;
            case 'K': s[i] = 'M'; break;
            case 'M': s[i] = 'K'; break;
            case 'N': s[i] = 'N'; break;
            case 'R': s[i] = 'Y'; break;
            case 'S': s[i] = 'S'; break;
            case 'T': s[i] = 'A'; break;
            case 'U': s[i] = 'A'; break;
            case 'V': s[i] = 'B'; break;
            case 'W': s[i] = 'W'; break;
            case 'X': s[i] = 'X'; break;
            case 'Y': s[i] = 'R'; break;
            case 'a': s[i] = 'u'; break;
            case 'b': s[i] = 'v'; break;
            case 'c': s[i] = 'g'; break;
            case 'd': s[i] = 'h'; break;
            case 'g': s[i] = 'c'; break;
            case 'h': s[i] = 'd'; break;
            case 'k': s[i] = 'm'; break;
            case 'm': s[i] = 'k'; break;
            case 'n': s[i] = 'n'; break;
            case 'r': s[i] = 'y'; break;
            case 's': s[i] = 's'; break;
            case 't': s[i] = 'a'; break;
            case 'u': s[i] = 'a'; break;
            case 'v': s[i] = 'b'; break;
            case 'w': s[i] = 'w'; break;
            case 'x': s[i] = 'x'; break;
            case 'y': s[i] = 'r'; break;
            default: break;
        }
    }

    if (PyByteArray_Check(data)) Py_RETURN_NONE;

    return data;
}

PyDoc_STRVAR(Seq_append_doc,
"Add a letter to the sequence object.\n"
"\n"
">>> my_seq = MutableSeq('ACTCGACGTCG')\n"
">>> my_seq.append('A')\n"
">>> my_seq\n"
"MutableSeq('ACTCGACGTCGA')\n"
"\n"
"No return value.\n"
"\n"
"A ValueError will be raised if the sequence is immutable.\n");

static PyObject *
Seq_append(SeqObject *self, PyObject *arg)
{
    char letter;
    PyObject *data = self->data;
    Py_ssize_t n = Py_SIZE(data);

    if (!PyByteArray_Check(data)) {
        PyErr_SetString(PyExc_ValueError, "sequence is immutable");
        return NULL;
    }

    if (n == PY_SSIZE_T_MAX) {
        PyErr_SetString(PyExc_OverflowError,
                        "cannot add more letters to sequence");
        return NULL;
    }

    if (PyUnicode_Check(arg)) {
        if (PyUnicode_READY(arg) == -1) return NULL;
        if (PyUnicode_GET_LENGTH(arg) == 1) {
            arg = PyUnicode_AsASCIIString(arg);
            if (!arg) return NULL;
            letter = PyBytes_AS_STRING(arg)[0];
            Py_DECREF(arg);
            if (PyByteArray_Resize(data, n + 1) < 0) return NULL;
            PyByteArray_AS_STRING(data)[n] = letter;
            Py_RETURN_NONE;
        }
    }

    PyErr_SetString(PyExc_ValueError, "expected a single letter");
    return NULL;
}

PyDoc_STRVAR(Seq_extend_doc,
"Extend a sequence object by a string or sequence.\n"
"\n"
">>> my_seq = MutableSeq('ACTCGACGTCG')\n"
">>> my_seq.extend('A')\n"
">>> my_seq\n"
"MutableSeq('ACTCGACGTCGA')\n"
">>> my_seq.extend('TTT')\n"
">>> my_seq\n"
"MutableSeq('ACTCGACGTCGATTT')\n"
"\n"
"No return value.\n"
"\n"
"A ValueError will be raised if the sequence is immutable.\n");

static PyObject *
Seq_extend(SeqObject *self, PyObject *arg)
{
    char *letters;
    PyObject *data = self->data;
    Py_ssize_t n = Py_SIZE(data);
    Py_ssize_t m;

    if (!PyByteArray_Check(data)) {
        PyErr_SetString(PyExc_ValueError, "sequence is immutable");
        return NULL;
    }

    if (PyUnicode_Check(arg)) {
        if (PyUnicode_READY(arg) == -1) return NULL;
        m = PyUnicode_GET_LENGTH(arg);
        if (n >= PY_SSIZE_T_MAX - m) {
            PyErr_SetString(PyExc_OverflowError,
                            "cannot add letters to sequence");
            return NULL;
        }

        arg = PyUnicode_AsASCIIString(arg);
        if (!arg) return NULL;
        letters = PyBytes_AS_STRING(arg);
        if (PyByteArray_Resize(data, n + m) < 0) {
            Py_DECREF(arg);
            return NULL;
        }
        memcpy(PyByteArray_AS_STRING(data) + n, letters, m);
        Py_DECREF(arg);
        Py_RETURN_NONE;
    }

    if ((PyObject*)self == arg) {
        /* special case; cannot be handled by the buffer code below */
        if (n >= PY_SSIZE_T_MAX - n) {
            PyErr_SetString(PyExc_OverflowError,
                            "cannot add letters to sequence");
            return NULL;
        }
        if (PyByteArray_Resize(data, 2*n) < 0) return NULL;
        letters = PyByteArray_AS_STRING(data);
        memcpy(letters + n, letters, n);
        Py_RETURN_NONE;
    }

    if (PyObject_IsInstance(arg, (PyObject*)&SeqType)) {
        Py_buffer view;
        if (PyObject_GetBuffer(arg, &view, PyBUF_STRIDES | PyBUF_FORMAT) < 0)
            return NULL;
        if (view.ndim != 1 || strcmp(view.format, "B") != 0) {
            PyBuffer_Release(&view);
            PyErr_SetString(PyExc_RuntimeError,
                            "expected a 1-dimensional sequence of bytes");
            return NULL;
        }
        m = view.len;
        if (n >= PY_SSIZE_T_MAX - m) {
            PyErr_SetString(PyExc_OverflowError,
                            "cannot add letters to sequence");
            return NULL;
        }
        letters = view.buf;
        if (PyByteArray_Resize(data, n + m) < 0) {
            PyBuffer_Release(&view);
            return NULL;
        }
        if (view.strides[0] == 1)
            memcpy(PyByteArray_AS_STRING(data) + n, letters, m);
        else if (view.strides[0] == 0)
            memset(PyByteArray_AS_STRING(data) + n, letters[0], m);
        else {
            PyErr_Format(PyExc_RuntimeError,
                         "unexpected stride %zd in Seq object",
                         view.strides[0]);
            PyBuffer_Release(&view);
            return NULL;
        }
        PyBuffer_Release(&view);
        Py_RETURN_NONE;
    }

    PyErr_SetString(PyExc_ValueError, "expected a string or a Seq object");
    return NULL;
}

PyDoc_STRVAR(Seq_insert_doc,
"Insert a letter into the sequence object at the specified index.\n"
"\n"
">>> my_seq = MutableSeq('ACTCGACGTCG')\n"
">>> my_seq.insert(0,'A')\n"
">>> my_seq\n"
"MutableSeq('AACTCGACGTCG')\n"
">>> my_seq.insert(8,'G')\n"
">>> my_seq\n"
"MutableSeq('AACTCGACGGTCG')\n"
"\n"
"No return value.\n"
"\n"
"A ValueError will be raised if the sequence is immutable.\n");

static PyObject *
Seq_insert(SeqObject *self, PyObject *args)
{
    char letter;
    char *buf;
    PyObject *data = self->data;
    Py_ssize_t i;
    Py_ssize_t n = Py_SIZE(data);

    if (!PyByteArray_Check(data)) {
        PyErr_SetString(PyExc_ValueError, "sequence is immutable");
        return NULL;
    }

    if (n == PY_SSIZE_T_MAX) {
        PyErr_SetString(PyExc_OverflowError,
                        "cannot add more letters to sequence");
        return NULL;
    }

    if (!PyArg_ParseTuple(args, "ns:insert", &i, &buf)) return NULL;

    letter = buf[0];
    if (letter == '\0' || (int)letter >= 128) {
        PyErr_SetString(PyExc_ValueError, "only ASCII letters are allowed");
        return NULL;
    }
    if (buf[1] != '\0') {
        PyErr_SetString(PyExc_ValueError, "expected a single letter");
        return NULL;
    }

    if (PyByteArray_Resize(data, n + 1) < 0) return NULL;
    buf = PyByteArray_AS_STRING(data);
    if (i < 0) {
        i += n;
        if (i < 0) i = 0;
    }
    else if (i > n) i = n;
    memmove(buf + i + 1, buf + i, n - i);
    buf[i] = letter;

    Py_RETURN_NONE;
}

PyDoc_STRVAR(Seq_pop_doc,
"Remove the letter at given index and return it.\n"
"\n"
">>> my_seq = MutableSeq('ACTCGACGTCG')\n"
">>> my_seq.pop()\n"
"'G'\n"
">>> my_seq\n"
"MutableSeq('ACTCGACGTC')\n"
">>> my_seq.pop()\n"
"'C'\n"
">>> my_seq\n"
"MutableSeq('ACTCGACGT')\n");

static PyObject *
Seq_pop(SeqObject *self, PyObject *args)
{
    char letter;
    char* buf;
    PyObject *data = self->data;
    const Py_ssize_t n = Py_SIZE(data);
    Py_ssize_t i = n - 1;

    if (!PyByteArray_Check(data)) {
        PyErr_SetString(PyExc_ValueError, "sequence is immutable");
        return NULL;
    }

    if (!PyArg_ParseTuple(args, "|n:pop", &i)) return NULL;

    if (i == -1 && PyErr_Occurred()) NULL;
    if (i < 0) i += n;
    if (i < 0 || i >= n) {
        PyErr_SetString(PyExc_IndexError, "sequence index out of range");
        return NULL;
    }

    buf = PyByteArray_AS_STRING(data);
    letter = buf[i];

    memmove(buf + i, buf + i + 1, n - i - 1);

    if (PyByteArray_Resize(data, n - 1) < 0) return NULL;

    return PyUnicode_FromStringAndSize(&letter, 1);
}

PyDoc_STRVAR(Seq_count_doc,
"Return a non-overlapping count, like that of a python string.\n"
"\n"
"This behaves like the python string method of the same name,\n"
"which does a non-overlapping count!\n"
"\n"
"For an overlapping search, use the count_overlap() method.\n"
"\n"
"Returns an integer, the number of occurrences of substring\n"
"argument sub in the (sub)sequence given by [start:end].\n"
"Optional arguments start and end are interpreted as in slice\n"
"notation.\n"
"\n"
"Arguments:\n"
" - sub - a string or another Seq object to look for\n"
" - start - optional integer, slice start\n"
" - end - optional integer, slice end\n"
"\n"
"e.g.\n"
"\n"
">>> from Bio.Seq import Seq\n"
">>> my_seq = Seq(\"AAAATGA\")\n"
">>> print(my_seq.count(\"A\"))\n"
"5\n"
">>> print(my_seq.count(\"ATG\"))\n"
"1\n"
">>> print(my_seq.count(Seq(\"AT\")))\n"
"1\n"
">>> print(my_seq.count(\"AT\", 2, -1))\n"
"1\n"
"\n"
"HOWEVER, please note because python strings and Seq objects (and\n"
"MutableSeq objects) do a non-overlapping search, this may not give\n"
"the answer you expect:\n"
"\n"
">>> \"AAAA\".count(\"AA\")\n"
"2\n"
">>> print(Seq(\"AAAA\").count(\"AA\"))\n"
"2\n"
"\n"
"An overlapping search, as implemented in .count_overlap(),\n"
"would give the answer as three!\n"
"\n");

static int
index_converter(PyObject* argument, void* pointer)
{
    Py_ssize_t index;
    if (argument == Py_None) return 1;
    index = PyNumber_AsSsize_t(argument, NULL);
    if (index == -1 && PyErr_Occurred()) return 0;
    *((Py_ssize_t*)pointer) = index;
    return 1;
}

static PyObject *
Seq_count(SeqObject *self, PyObject *args)
{
    PyObject *data = self->data;
    Py_ssize_t start = 0;
    Py_ssize_t end = PY_SSIZE_T_MAX;
    PyObject *sub;
    PyObject* result = NULL;

    if (!PyArg_ParseTuple(args, "O|O&O&:count", &sub,
                                index_converter, &start,
                                index_converter, &end)) return NULL;

    if (PyBytes_Check(sub) || PyByteArray_Check(sub))
        Py_INCREF(sub);
    else {
        if (PyUnicode_Check(sub)) sub = PyUnicode_AsASCIIString(sub);
        else sub = PyObject_Bytes(sub);
        if (!sub) return NULL;
    }

    if (PyObject_CheckBuffer(data)) {
        Py_buffer view;
        if (PyObject_GetBuffer(data, &view, PyBUF_STRIDES | PyBUF_FORMAT) == 0) {
            Py_ssize_t stride = view.strides[0];
            const char* s = view.buf;
            const char letter = *s;
            Py_ssize_t length = view.len;
            PyBuffer_Release(&view);
            if (stride == 0) {
                if (end < 0) end += length;
                else if (end > length) end = length;
                if (start < 0) start += length;
                if (start < 0) start = 0;
                if (start < end) {
                    Py_ssize_t i;
                    if (PyObject_GetBuffer(sub, &view, PyBUF_STRIDES | PyBUF_FORMAT) < 0)
                        goto exit;
                    s = view.buf;
                    stride = view.strides[0];
                    for (i = 0; i < view.len; i++) {
                        if (s[i*stride] != letter)  {
                            result = PyLong_FromLong(0);
                            break;
                        }
                    }
                    if (!result)
                        result = PyLong_FromLong((end-start)/view.len);
                    PyBuffer_Release(&view);
                }
                else
                    result = PyLong_FromLong(0);
                goto exit;
            }
        }
        else PyErr_Clear();
    }

    if (!PyBytes_Check(data) && ! PyByteArray_Check(data)) {
        if (!PySequence_Check(data)) {
            PyErr_SetString(PyExc_RuntimeError,
                            "data should support the sequence protocol");
            goto exit;
        }
        self->data = PySequence_GetSlice(data, 0, PY_SSIZE_T_MAX);
        if (!self->data) {
            self->data = data;
            goto exit;
        }
        Py_DECREF(data);
    }
    result = PyObject_CallMethod(data, "count", "Onn", sub, start, end);

exit:
    Py_DECREF(sub);
    return result;
}

static PyMethodDef Seq_methods[] = {
    {"__reduce__", (PyCFunction)Seq_reduce, METH_NOARGS, Seq_reduce_doc},
    {"reverse", (PyCFunction)Seq_reverse, METH_NOARGS, Seq_reverse_doc},
    {"complement", (PyCFunction)Seq_complement, METH_NOARGS, Seq_complement_doc},
    {"rna_complement", (PyCFunction)Seq_rna_complement, METH_NOARGS, Seq_rna_complement_doc},
    {"append", (PyCFunction)Seq_append, METH_O, Seq_append_doc},
    {"extend", (PyCFunction)Seq_extend, METH_O, Seq_extend_doc},
    {"insert", (PyCFunction)Seq_insert, METH_VARARGS, Seq_insert_doc},
    {"pop", (PyCFunction)Seq_pop, METH_VARARGS, Seq_pop_doc},
    {"count", (PyCFunction)Seq_count, METH_VARARGS, Seq_count_doc},
    {NULL}  /* Sentinel */
};

static PyObject *
Seq_id_get(SeqObject *self, void *closure)
{
    if (!self->id) return PyUnicode_New(0, 127);
    Py_INCREF(self->id);
    return self->id;
}

static int
Seq_id_set(SeqObject *self, PyObject *value, void *closure)
{
    if (value) {
        if (PyUnicode_Check(value)) {
            if (PyUnicode_CompareWithASCIIString(value, "") == 0) value = NULL;
            else Py_INCREF(value);
        }
        else if (value == Py_None) {
            Py_INCREF(value);
        }
        else {
            PyErr_Format(PyExc_TypeError,
                "attribute id requires a string or None (received type %s)",
                Py_TYPE(value)->tp_name);
            return -1;
        }
    }
    Py_XDECREF(self->id);
    self->id = value;
    return 0;
}

static PyObject *
Seq_name_get(SeqObject *self, void *closure)
{
    if (!self->name) return PyUnicode_New(0, 127);
    Py_INCREF(self->name);
    return self->name;
}

static int
Seq_name_set(SeqObject *self, PyObject *value, void *closure)
{
    if (value) {
        if (PyUnicode_Check(value)) {
            if (PyUnicode_CompareWithASCIIString(value, "") == 0) value = NULL;
            else Py_INCREF(value);
        }
        else if (value == Py_None) {
            Py_INCREF(value);
        }
        else {
            PyErr_Format(PyExc_TypeError,
                "attribute name requires a string or None (received type %s)",
                Py_TYPE(value)->tp_name);
            return -1;
        }
    }
    Py_XDECREF(self->name);
    self->name = value;
    return 0;
}

static PyObject *
Seq_description_get(SeqObject *self, void *closure)
{
    if (!self->description) return PyUnicode_New(0, 127);
    Py_INCREF(self->description);
    return self->description;
}

static int
Seq_description_set(SeqObject *self, PyObject *value, void *closure)
{
    if (value) {
        if (PyUnicode_Check(value)) {
            if (PyUnicode_CompareWithASCIIString(value, "") == 0) value = NULL;
            else Py_INCREF(value);
        }
        else if (value == Py_None) {
            Py_INCREF(value);
        }
        else {
            PyErr_Format(PyExc_TypeError,
                "attribute description requires a string or None (received type %s)",
                Py_TYPE(value)->tp_name);
            return -1;
        }
    }
    Py_XDECREF(self->description);
    self->description = value;
    return 0;
}

static PyObject *
Seq_annotations_get(SeqObject *self, void *closure)
{
    if (!self->annotations) {
        self->annotations = PyDict_New();
        if (!self->annotations) return NULL;
    }
    Py_INCREF(self->annotations);
    return self->annotations;
}

static int
Seq_annotations_set(SeqObject *self, PyObject *value, void *closure)
{
    if (value) {
        if (!PyDict_Check(value)) {
            PyErr_Format(PyExc_TypeError,
                "attribute annotations requires a dictionary (received type %s)",
                Py_TYPE(value)->tp_name);
            return -1;
        }
        Py_INCREF(value);
    }
    Py_XDECREF(self->annotations);
    self->annotations = value;
    return 0;
}

static PyObject *
Seq_features_get(SeqObject *self, void *closure)
{
    if (!self->features) {
        self->features = PyList_New(0);
        if (!self->features) return NULL;
    }
    Py_INCREF(self->features);
    return self->features;
}

static int
Seq_features_set(SeqObject *self, PyObject *value, void *closure)
{
    if (value) {
        if (!PyList_Check(value)) {
            PyErr_Format(PyExc_TypeError,
                "attribute features requires a list (received type %s)",
                Py_TYPE(value)->tp_name);
            return -1;
        }
        Py_INCREF(value);
    }
    Py_XDECREF(self->features);
    self->features = value;
    return 0;
}

static PyObject *
Seq_letter_annotations_get(SeqObject *self, void *closure)
{
    if (!self->letter_annotations) {
        /* Let Bio.Seq.Seq create the restricted dictionary if needed. */
        PyErr_SetString(PyExc_AttributeError,
                        "Seq object has no attribute 'letter_annotations'");
        return NULL;
    }
    Py_INCREF(self->letter_annotations);
    return self->letter_annotations;
}

static int
Seq_letter_annotations_set(SeqObject *self, PyObject *value, void *closure)
{
    if (value) {
        if (!PyDict_Check(value)) {
            PyErr_Format(PyExc_TypeError,
                "attribute letter_annotations requires a dictionary (received type %s)",
                Py_TYPE(value)->tp_name);
            return -1;
        }
        Py_INCREF(value);
    }
    Py_XDECREF(self->letter_annotations);
    self->letter_annotations = value;
    return 0;
}

static PyObject *
Seq_dbxrefs_get(SeqObject *self, void *closure)
{
    if (!self->dbxrefs) {
        self->dbxrefs = PyList_New(0);
        if (!self->dbxrefs) return NULL;
    }
    Py_INCREF(self->dbxrefs);
    return self->dbxrefs;
}

static int
Seq_dbxrefs_set(SeqObject *self, PyObject *value, void *closure)
{
    if (value) {
        if (!PyList_Check(value)) {
            PyErr_Format(PyExc_TypeError,
                "attribute dbxrefs requires a list (received type %s)",
                Py_TYPE(value)->tp_name);
            return -1;
        }
        Py_INCREF(value);
    }
    Py_XDECREF(self->dbxrefs);
    self->dbxrefs = value;
    return 0;
}

static PyGetSetDef Seq_getset[] = {
    {"id", (getter)Seq_id_get, (setter)Seq_id_set, "", NULL},
    {"name", (getter)Seq_name_get, (setter)Seq_name_set, "", NULL},
    {"description", (getter)Seq_description_get, (setter)Seq_description_set, "", NULL},
    {"annotations", (getter)Seq_annotations_get, (setter)Seq_annotations_set, "", NULL},
    {"features", (getter)Seq_features_get, (setter)Seq_features_set, "", NULL},
    {"dbxrefs", (getter)Seq_dbxrefs_get, (setter)Seq_dbxrefs_set, "", NULL},
    {"letter_annotations", (getter)Seq_letter_annotations_get, (setter)Seq_letter_annotations_set, "", NULL},
    {NULL}  /* Sentinel */
};

PyDoc_STRVAR(Seq_doc, "Seq() -> Seq");

static PyTypeObject SeqType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "Seq",                                      /* tp_name */
    sizeof(SeqObject),                          /* tp_basicsize */
    0,                                          /* tp_itemsize */
    (destructor)Seq_dealloc,                    /* tp_dealloc */
    0,                                          /* tp_print */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_reserved */
    (reprfunc)Seq_repr,                         /* tp_repr */
    &Seq_as_number,                             /* tp_as_number */
    0,                                          /* tp_as_sequence */
    &Seq_as_mapping,                            /* tp_as_mapping */
    0,                                          /* tp_hash */
    0,                                          /* tp_call */
    (reprfunc)Seq_str,                          /* tp_str */
    0,                                          /* tp_getattro */
    0,                                          /* tp_setattro */
    &Seq_as_buffer,                             /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,   /* tp_flags */
    Seq_doc,                                    /* tp_doc */
    0,                                          /* tp_traverse */
    0,                                          /* tp_clear */
    (richcmpfunc)Seq_richcompare,               /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    Seq_methods,                                /* tp_methods */
    0,                                          /* tp_members */
    Seq_getset,                                 /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    0,                                          /* tp_init */
    0,                                          /* tp_alloc */
    (newfunc)Seq_new,                           /* tp_new */
};

static struct PyModuleDef moduledef = {
        PyModuleDef_HEAD_INIT,
        "_seqobject",
        NULL,
        -1,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
};

PyObject *
PyInit__seqobject(void)
{
    PyObject *module;

    if (PyType_Ready(&SeqType) < 0)
        return NULL;

    module = PyModule_Create(&moduledef);
    if (module == NULL) return NULL;

    Py_INCREF(&SeqType);
    if (PyModule_AddObject(module, "Seq", (PyObject*) &SeqType) < 0) {
        Py_DECREF(module);
        Py_DECREF(&SeqType);
        return NULL;
    }

    return module;
}

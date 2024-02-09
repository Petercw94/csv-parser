#include <stdio.h>
#include <stdlib.h>
#define PY_SSIZE_T_CLEAN
#include "../include/ccsv.h"
#include <Python.h>
#include <structmember.h>

#define LINE_ENDING '\n'
#define ON 1
#define OFF 0

typedef struct {
  PyObject_HEAD PyObject *file_name;
  int headers_exist;
  PyObject *headers;
} CsvObject;

static PyObject *get_headers(CsvObject *self);
static int get_column_index(PyObject *dict, PyObject *key);
static PyObject *convert_row_to_tuple(Row row);
static PyObject *convert_row_to_dict(Row row);
static int get_column_index(PyObject *dict, PyObject *key);
static int search_column(char *column, const char *pattern, int pattern_length);
static PyObject *filter(CsvObject *self, PyObject *args, PyObject *kwds);

static void Csv_dealloc(CsvObject *self) {
  Py_XDECREF(self->file_name);
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *Csv_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
  CsvObject *self;
  self = (CsvObject *)type->tp_alloc(type, 0);
  if (self != NULL) {
    self->file_name = PyUnicode_FromString("");
    if (self->file_name == NULL) {
      Py_DECREF(self);
      return NULL;
    }
  }
  return (PyObject *)self;
}

static int Csv_init(CsvObject *self, PyObject *args, PyObject *kwds) {
  static char *kwdlist[] = {"file_name", "headers_exist", NULL};
  PyObject *file_name = NULL, *tmp, *headers;
  self->headers_exist = 1; // default to true
  if (!PyArg_ParseTupleAndKeywords(args, kwds, "U|p", kwdlist, &file_name,
                                   &self->headers_exist)) {
    return -1;
  }

  if (file_name) {
    tmp = self->file_name;
    Py_INCREF(file_name);
    self->file_name = file_name;
    Py_XDECREF(tmp);
  }

  if (self->headers_exist) {

    headers = get_headers(self);

    if (headers != NULL) {
      Py_INCREF(headers);
      self->headers = headers;
    }
  }

  return 0;
}

static PyMemberDef Csv_members[] = {
    {"file_name", T_OBJECT_EX, offsetof(CsvObject, file_name), 0,
     "full path to file."},
    {"headers", T_OBJECT_EX, offsetof(CsvObject, headers), 0,
     "tuple of file headers"},
    {NULL} /* Sentinal */
};

static PyObject *get_headers(CsvObject *self) {
  // 1.
  const char *file_path = PyUnicode_AsUTF8(self->file_name);

  // 2.
  FILE *fp = fopen(file_path, "r");
  if (fp == NULL) {
    // TODO: make the error string more descriptive
    PyErr_SetString(PyExc_IOError, "error opening the provided file");
  }

  // 3.
  Row row;
  row = parseRow(fp);
  // TODO: check that the file closed properly
  int close = fclose(fp);
  // 4.
  PyObject *py_dict = convert_row_to_dict(row);

  return py_dict;
}

static PyMethodDef Csv_methods[] = {
    {"get_headers", (PyCFunction)get_headers, METH_NOARGS,
     "Return the first row of a csv file as the headers."},
    {"filter", (PyCFunction)filter, METH_VARARGS | METH_KEYWORDS,
     "Search the provided column for the provided pattern. Return the rows "
     "that have a match"},
    {NULL}};

static PyTypeObject CsvType = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "ccsv.Csv",
    .tp_doc = PyDoc_STR("CSV Parser class"),
    .tp_basicsize = sizeof(CsvObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = Csv_new,
    .tp_init = (initproc)Csv_init,
    .tp_dealloc = (destructor)Csv_dealloc,
    .tp_members = Csv_members,
    .tp_methods = Csv_methods,
};

static PyModuleDef ccsvmodule = {
    PyModuleDef_HEAD_INIT,
    .m_name = "ccsv",
    .m_doc = "Custom CSV parser that allows sql-like functionality.",
    .m_size = -1,
};

PyMODINIT_FUNC PyInit_ccsv(void) {
  PyObject *m;
  if (PyType_Ready(&CsvType) < 0) {
    return NULL;
  }

  m = PyModule_Create(&ccsvmodule);
  if (m == NULL) {
    return NULL;
  }

  Py_INCREF(&CsvType);
  if (PyModule_AddObject(m, "Csv", (PyObject *)&CsvType) < 0) {
    Py_DECREF(&CsvType);
    Py_DECREF(m);
    return NULL;
  }

  return m;
}

/*
 * Convert a Row object into a Python tuple
 * */
static PyObject *convert_row_to_tuple(Row row) {

  PyObject *py_tuple = PyTuple_New(row.columnCount);

  /* Loop through the array and:
   * - convert each string into a python string PyObject*
   * - add the string object to the tuple. */
  PyObject *column_string, *column_index;

  for (long i = 0; i < row.columnCount; ++i) {
    column_string =
        PyUnicode_FromString(row.columns[i]); // create a PyObject Unicode type
                                              // from the string literals
    column_index = PyLong_FromLong(i);
    int error = PyTuple_SetItem(py_tuple, i, column_string);
    if (error != 0) {
      fprintf(stderr, "Error adding PyUnicode Object to PyDict.\n");
      exit(EXIT_FAILURE);
    }
  }

  return py_tuple;
}

static PyObject *convert_row_to_dict(Row row) {

  PyObject *py_dict = PyDict_New();

  /* Loop through the array and:
   * - convert each string into a python string PyObject*
   * - add the string object to the tuple. */
  PyObject *column_string, *column_index;

  for (long i = 0; i < row.columnCount; ++i) {
    column_string =
        PyUnicode_FromString(row.columns[i]); // create a PyObject Unicode type
                                              // from the string literals
    column_index = PyLong_FromLong(i);
    int error = PyDict_SetItem(py_dict, column_string, column_index);
    if (error != 0) {
      fprintf(stderr, "Error adding PyUnicode Object to PyDict.\n");
      exit(EXIT_FAILURE);
    }
  }
  return py_dict;
}

/*
 * Return the index for the provided column name (str)*/
static int get_column_index(PyObject *dict, PyObject *key) {
  PyObject *py_value = PyDict_GetItem(dict, key);

  if (py_value == NULL) {
    PyErr_SetString(
        PyExc_ValueError,
        "Provided header does not match any headers found in file. Use the "
        ".headers() method for a list of headers found in file.");
    return -1;
  }

  int index = (int)PyLong_AsLong(py_value);
  return index;
}

/*
 * Search a single column value for a provided pattern
 * */
static int search_column(char *column, const char *pattern,
                         int pattern_length) {
  int col_len = strlen(column); // find the length of the col
  int pi, found; // pi will indicate the current index of the pattern
  pi = found = 0;
  // loop through the column
  for (int i = 0; i < col_len; ++i) {
    while (column[i] == pattern[pi]) {
      ++i;
      ++pi;
    }
    if (pi == pattern_length) {
      found = 1;
    }

    pi = 0;
  }

  return found;
}

static PyObject *filter(CsvObject *self, PyObject *args, PyObject *kwds) {
  // default tuple result set size to 5
  int file_size = 100;
  PyObject **results_array = malloc(
      sizeof(PyObject *) * file_size); // dynamically allocate the array so it
                                       // can be freed when we are done with it.
  PyObject *results;                   // final tuple object
  Py_ssize_t result_count, zero_index;
  result_count = zero_index =
      0;     // count the results, if it gets higher than 5 than we
             // need to reallocate our tuple to a bigger size
  int found; // will indicate if a Row has a match for the pattern
  // parse the parameters
  static char *kwlist[] = {"column", "pattern", NULL};

  PyObject *column, *pattern;
  if (!PyArg_ParseTupleAndKeywords(args, kwds, "UU", kwlist, &column,
                                   &pattern)) {
    PyErr_SetString(PyExc_Exception, "error parsing the passed paramters.");
    return results;
  }
  const char *pattern_str = PyUnicode_AsUTF8(pattern);
  int pattern_length =
      strlen(pattern_str); // find the len of the pattern string
  int index = get_column_index(self->headers, column);
  if (index < 0) {
    return results; // NOTE: get_column_index already raises a python error, so
                    // just need to terminate execution here by sending an empty
                    // tuple
  }

  // open a new file pointer for the file
  const char *file_path = PyUnicode_AsUTF8(self->file_name);

  // 2.
  FILE *fp = fopen(file_path, "r");
  // TODO: check that the file opened properly
  int tuple_index = 0;
  int tuple_write_success;
  int row_num = 0;
  for (;;) {
    Row row = parseRow(fp);
    found = search_column(row.columns[index], pattern_str, pattern_length);
    if (found == 1) {
      PyObject *row_tuple = convert_row_to_tuple(row);
      result_count++;
      if (result_count >= file_size) {
        file_size = file_size + 100;
        results_array = realloc(results_array, sizeof(PyObject *) * file_size);
        if (results_array == NULL) {
          PyErr_SetString(PyExc_MemoryError,
                          "error allocating memory for results array.");
        }
      }
      results_array[tuple_index++] = row_tuple;
    }
    if (row.lastRow == 1) {
      break;
    }
  }
  fclose(fp);
  results = PyList_New(tuple_index);
  for (int i = 0; i < tuple_index; ++i) {
    int error = PyList_SetItem(results, i, results_array[i]);
    if (error != 0) {
      if (error == -1) {
        PyErr_SetString(PyExc_IndexError, "index out of bounds.");
        free(results_array);
        return results;
      }
      PyErr_SetString(PyExc_Exception,
                      "error while converting to Python list.");
      free(results_array);
      return results;
    }
  }
  free(results_array);
  return results;
}

/*
 * c_count -> the character count of the column
 * c_string -> a pointer to the character string representation of
 * */
char *parseColumn(FILE *fp, int c_count) {
  int c;
  int offset = -c_count;
  offset--; // remove one from the offset to move cursor correctly

  // allocate memory for the column string
  char *s = (char *)malloc((c_count + 1) * sizeof(char));
  if (s == NULL) {
    fprintf(stderr, "Error allocating memory for column string.\n");
  }
  int error = fseek(fp, offset, SEEK_CUR);
  if (error != 0) {
    fprintf(stderr,
            "Error adjusting the File Pointer while parsing the column\n");
    exit(EXIT_FAILURE);
  }
  for (int i = 0; i < c_count; ++i) {
    c = fgetc(fp);
    if (c == '\r') {
      break; // \r indicates the end of the row in some formats I guess?
    }
    s[i] = c;
  }
  s[c_count] = '\0'; // terminate the string
  return s;
}

/* Parse the columns for the next row in the file buffer.
 * Column values parsed as strings and written in order to the provided row
 * array Returns: an integer indicating the total number of columns in the
 * provided row */
Row parseRow(FILE *fp) {

  int c, colStateQuotes, prevChar, colCharCount, error, colCount;
  colStateQuotes = OFF;

  colCount = colCharCount = prevChar = 0;
  Row row;
  row.lastRow = 0;

  // allocate an array of the default column size
  row.columns = malloc(DEFAULT_COL_NUM * sizeof(char *));
  if (row.columns == NULL) {
    fprintf(stderr,
            "Error: There was an error allocating memory for the row array.\n");
  }

  while ((c = fgetc(fp)) != LINE_ENDING) {

    /* if line ending or end of file hit, parse the column then break the loop
     */
    if (c == EOF) {
      row.columns[colCount++] = parseColumn(fp, colCharCount);
      row.lastRow = 1;
      break;
    }
    /* If a " appears and we aren't currently in a
     * column quote state, it indicates that its the
     * beginning of a new col that is surrounded by
     * quotes. See RFC standards for explanation */
    else if (c == '"' && colStateQuotes == 0) {
      colStateQuotes = ON; // this state indicates whether we are currently in a
                           // data state or not
    }

    else if (c == ',') {
      /* if a comma appears and we aren't in a quoted data column,
       * it would indicate that its the end of the column */
      if (colStateQuotes == 0) {
        row.columns[colCount++] = parseColumn(fp, colCharCount);
        colCharCount = 0; // reset the char counter for the next col
        error = fseek(fp, 1, SEEK_CUR);
        if (error != 0) {
          fprintf(stderr,
                  "Error advancing file pointer after parsing column.\n");
          exit(EXIT_FAILURE);
        }
      }

      /* If a comma appears and the program is in the quoted
       * data col state, a check needs to be made to the previous
       * character. If the previous character is a double quote, it would
       * signify the end of the data state, which would also
       * indicate the end of the column. */
      else if (prevChar == '"') {
        row.columns[colCount++] = parseColumn(fp, colCharCount);
        colCharCount = 0; // reset the char counter for the next col
        error = fseek(fp, 1, SEEK_CUR);
        if (error != 0) {
          fprintf(stderr,
                  "Error advancing file pointer after parsing column.\n");
          exit(EXIT_FAILURE);
        }
      }

      /* If the character does not signify the end of the column
       * it needs to be counted so the parseCol function knows how
       * far back it needs to move the file pointer to allocate enough
       * memory and parse the column value*/
      else {
        ++colCharCount;
      }
    }

    else {
      ++colCharCount;
    }
    prevChar = c;
  }
  row.columns[colCount++] = parseColumn(fp, colCharCount);
  row.columnCount = colCount;

  // advance the cursor to the next line
  error = fseek(fp, 1, SEEK_CUR);
  if (error != 0) {
    fprintf(stderr, "Error advancing file pointer after parsing column.\n");
    exit(EXIT_FAILURE);
  }
  return row;
}

void parseFile(FILE *fp) {
  Row row;
  // loop through the whole file
  for (;;) {

    row = parseRow(fp);

    for (int i = 0; i < row.columnCount; ++i) {
      printf("%s", row.columns[i]);
    }
    printf("\n");

    free(row.columns);

    if (row.lastRow == 1) {
      break;
    }
  }
}

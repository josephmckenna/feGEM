/********************************************************************\

  Name:         tmodb.cxx
  Created by:   K.Olchanski

  Contents:     MIDAS implementation of TMVodb ODB interface

\********************************************************************/

#include <stdio.h>
#include <string.h> // strlen()
#include <assert.h>
#include <stdlib.h> // malloc()

#include "tmvodb.h"
#include "midas.h"

static std::string toString(int value)
{
   char buf[256];
   sprintf(buf, "%d", value);
   return buf;
}

#if 0

static int odbResizeArray(TMFE* mfe, const char*name, int tid, int size)
{
   int oldSize = odbReadArraySize(mfe, name);

   if (oldSize >= size)
      return oldSize;

   int status;
   HNDLE hkey;
   HNDLE hdir = 0;

   status = db_find_key(mfe->fDB, hdir, (char*)name, &hkey);
   if (status != SUCCESS) {
      mfe->Msg(MINFO, "odbResizeArray", "Creating \'%s\'[%d] of type %d", name, size, tid);
      
      status = db_create_key(mfe->fDB, hdir, (char*)name, tid);
      if (status != SUCCESS) {
         mfe->Msg(MERROR, "odbResizeArray", "Cannot create \'%s\' of type %d, db_create_key() status %d", name, tid, status);
         return -1;
      }
         
      status = db_find_key (mfe->fDB, hdir, (char*)name, &hkey);
      if (status != SUCCESS) {
         mfe->Msg(MERROR, "odbResizeArray", "Cannot create \'%s\', db_find_key() status %d", name, status);
         return -1;
      }
   }
   
   mfe->Msg(MINFO, "odbResizeArray", "Resizing \'%s\'[%d] of type %d, old size %d", name, size, tid, oldSize);

   status = db_set_num_values(mfe->fDB, hkey, size);
   if (status != SUCCESS) {
      mfe->Msg(MERROR, "odbResizeArray", "Cannot resize \'%s\'[%d] of type %d, db_set_num_values() status %d", name, size, tid, status);
      return -1;
   }
   
   return size;
}
#endif

#define LOCK_ODB()

class TMNullOdb: public TMVOdb
{
   TMVOdb* Chdir(const char* subdir, bool create)
   {
      return new TMNullOdb;
   }

   void RAInfo(const char* varname, int* num_elements, int* element_size) {};

   void RB(const char* varname, int index, bool   *value, bool create) {};
   void RI(const char* varname, int index, int    *value, bool create) {};
   void RD(const char* varname, int index, double *value, bool create) {};
   void RS(const char* varname, int index, std::string *value, bool create) {};
   void RU32(const char* varname, int index, uint32_t *value, bool create) {};

   void RBA(const char* varname, std::vector<bool> *value, bool create, int create_size) {};
   void RIA(const char* varname, std::vector<int> *value,  bool create, int create_size) {};
   void RDA(const char* varname, std::vector<double> *value,  bool create, int create_size) {};
   void RSA(const char* varname, std::vector<std::string> *value, bool create, int create_size, int string_size) {};

   void WB(const char* varname, bool v) {};
   void WI(const char* varname, int v)  {};
   void WD(const char* varname, double v) {};
   void WS(const char* varname, const char* v) {};

   void WBA(const char* varname, const std::vector<bool>& v) {};
   void WIA(const char* varname, const std::vector<int>& v) {};
   void WDA(const char* varname, const std::vector<double>& v) {};
   void WSA(const char* varname, const std::vector<std::string>& data, int odb_string_length) {};
};

TMVOdb* MakeNullOdb()
{
   return new TMNullOdb();
}

class TMOdb: public TMVOdb
{
public:
   HNDLE fDB = 0;
   std::string fRoot;
   bool fTrace = false;

public:
   TMOdb(HNDLE hDB, const char* root)
   {
      fDB = hDB;
      fRoot = root;
   }

   TMVOdb* Chdir(const char* subdir, bool create)
   {
      std::string path;
      path += fRoot;
      path += "/";
      path += subdir;
      TMOdb* p = new TMOdb(fDB, path.c_str());
      return p;
   }

   void RAInfo(const char* varname, int* num_elements, int* element_size)
   {
      std::string path;
      path += fRoot;
      path += "/";
      path += varname;

      LOCK_ODB();

      if (fTrace) {
         printf("Read ODB %s\n", path.c_str());
      }

      if (num_elements)
         *num_elements = 0;
      if (element_size)
         *element_size = 0;

      int status;
      HNDLE hkey;
      status = db_find_key(fDB, 0, path.c_str(), &hkey);
      if (status != DB_SUCCESS)
         return;
      
      KEY key;
      status = db_get_key(fDB, hkey, &key);
      if (status != DB_SUCCESS)
         return;

      if (num_elements)
         *num_elements = key.num_values;

      if (element_size)
         *element_size = key.item_size;
   }

   void RI(const char* varname, int index, int *value, bool create)
   {
      std::string path;
      path += fRoot;
      path += "/";
      path += varname;
   
      path += "[";
      path += toString(index);
      path += "]";
   
      LOCK_ODB();

      if (fTrace) {
         printf("Read ODB %s\n", path.c_str());
      }

      int size = sizeof(*value);
      int status = db_get_value(fDB, 0, path.c_str(), value, &size, TID_INT, create);
      
      if (status != DB_SUCCESS) {
         printf("RI: db_get_value status %d\n", status);
      }
   }

   void RU32(const char* varname, int index, uint32_t *value, bool create)
   {
      std::string path;
      path += fRoot;
      path += "/";
      path += varname;
   
      path += "[";
      path += toString(index);
      path += "]";
   
      LOCK_ODB();

      if (fTrace) {
         printf("Read ODB %s\n", path.c_str());
      }

      int size = sizeof(*value);
      int status = db_get_value(fDB, 0, path.c_str(), value, &size, TID_DWORD, create);
      
      if (status != DB_SUCCESS) {
         printf("RU32: db_get_value status %d\n", status);
      }
   }

   void RBA(const char* varname, std::vector<bool> *value, bool create, int create_size)
   {
      std::string path;
      path += fRoot;
      path += "/";
      path += varname;
   
      if (value) {
         value->clear();
      }

      LOCK_ODB();

      if (fTrace) {
         printf("Read ODB %s\n", path.c_str());
      }

      int num = 0;
      int esz = 0;

      RAInfo(varname, &num, &esz);

      if (num <= 0 || esz <= 0) {
         if (create) {
            num = create_size;
            esz = sizeof(BOOL);
         } else {
            return;
         }
      }

      assert(esz == sizeof(BOOL));

      int size = sizeof(BOOL)*num;

      assert(size > 0);

      BOOL* buf = (BOOL*)malloc(size);
      assert(buf);
      memset(buf, 0, size);
      int status = db_get_value(fDB, 0, path.c_str(), buf, &size, TID_BOOL, create);

      if (status != DB_SUCCESS) {
         printf("RBA: db_get_value status %d\n", status);
      }

      if (value) {
         for (int i=0; i<num; i++) {
            value->push_back(buf[i]);
         }
      }

      free(buf);
   }

   void RIA(const char* varname, std::vector<int> *value, bool create, int create_size)
   {
      std::string path;
      path += fRoot;
      path += "/";
      path += varname;

      if (value) {
         value->clear();
      }

      LOCK_ODB();

      if (fTrace) {
         printf("Read ODB %s\n", path.c_str());
      }

      int num = 0;
      int esz = 0;

      RAInfo(varname, &num, &esz);

      if (num <= 0 || esz <= 0) {
         if (create) {
            num = create_size;
            esz = sizeof(int);
         } else {
            return;
         }
      }

      assert(esz == sizeof(int));

      int size = sizeof(int)*num;

      assert(size > 0);

      int* buf = (int*)malloc(size);
      assert(buf);
      memset(buf, 0, size);
      int status = db_get_value(fDB, 0, path.c_str(), buf, &size, TID_INT, create);

      if (status != DB_SUCCESS) {
         printf("RIA: db_get_value status %d\n", status);
      }

      if (value) {
         for (int i=0; i<num; i++) {
            value->push_back(buf[i]);
         }
      }

      free(buf);
   }

   void RDA(const char* varname, std::vector<double> *value, bool create, int create_size)
   {
      std::string path;
      path += fRoot;
      path += "/";
      path += varname;

      if (value) {
         value->clear();
      }
   
      LOCK_ODB();

      if (fTrace) {
         printf("Read ODB %s\n", path.c_str());
      }

      int num = 0;
      int esz = 0;

      RAInfo(varname, &num, &esz);

      if (num <= 0 || esz <= 0) {
         if (create) {
            num = create_size;
            esz = sizeof(double);
         } else {
            return;
         }
      }

      assert(esz == sizeof(double));

      int size = sizeof(double)*num;

      assert(size > 0);

      double* buf = (double*)malloc(size);
      assert(buf);
      memset(buf, 0, size);
      int status = db_get_value(fDB, 0, path.c_str(), buf, &size, TID_DOUBLE, create);

      if (status != DB_SUCCESS) {
         printf("RDA: db_get_value status %d\n", status);
      }

      if (value) {
         for (int i=0; i<num; i++) {
            value->push_back(buf[i]);
         }
      }

      free(buf);
   }

   void RSA(const char* varname, std::vector<std::string> *value, bool create, int create_size, int string_size)
   {
      std::string path;
      path += fRoot;
      path += "/";
      path += varname;

      if (value) {
         value->clear();
      }

      LOCK_ODB();

      if (fTrace) {
         printf("Read ODB %s\n", path.c_str());
      }

      int num = 0;
      int esz = 0;

      RAInfo(varname, &num, &esz);

      bool create_first = false;

      if (num <= 0 || esz <= 0) {
         if (create) {
            num = create_size;
            esz = string_size;
            create_first = true;
         } else {
            return;
         }
      }

      int size = esz*num;

      assert(size > 0);

      char* buf = (char*)malloc(size);

      int status;
      if (create_first) {
         memset(buf, 0, size);
         status = db_set_value(fDB, 0, path.c_str(), buf, size, num, TID_STRING);
      }

      status = db_get_value(fDB, 0, path.c_str(), buf, &size, TID_STRING, create);

      if (status != DB_SUCCESS) {
         printf("RSA: db_get_value status %d\n", status);
      }

      for (int i=0; i<num; i++) {
         value->push_back(buf+esz*i);
      }

      free(buf);
   }

   void RD(const char* varname, int index, double *value, bool create)
   {
      std::string path;
      path += fRoot;
      path += "/";
      path += varname;
   
      path += "[";
      path += toString(index);
      path += "]";
   
      LOCK_ODB();

      if (fTrace) {
         printf("Read ODB %s\n", path.c_str());
      }

      int size = sizeof(*value);
      int status = db_get_value(fDB, 0, path.c_str(), value, &size, TID_DOUBLE, create);
      
      if (status != DB_SUCCESS) {
         printf("RD: db_get_value status %d\n", status);
      }
   }

   void RB(const char* varname, int index, bool *value, bool create)
   {
      std::string path;
      path += fRoot;
      path += "/";
      path += varname;

      path += "[";
      path += toString(index);
      path += "]";
   
      LOCK_ODB();

      if (fTrace) {
         printf("Read ODB %s\n", path.c_str());
      }

      BOOL xvalue = *value;

      int size = sizeof(xvalue);
      int status = db_get_value(fDB, 0, path.c_str(), &xvalue, &size, TID_BOOL, create);
      
      if (status != DB_SUCCESS) {
         printf("RB: db_get_value(%s) status %d\n", path.c_str(), status);
      }

      *value = xvalue;
   }

   void RS(const char* varname, int index, std::string* value, bool create)
   {
      std::string path;
      path += fRoot;
      path += "/";
      path += varname;
   
      path += "[";
      path += toString(index);
      path += "]";
   
      LOCK_ODB();

      if (fTrace) {
         printf("Read ODB %s\n", path.c_str());
      }

      int status = db_get_value_string(fDB, 0, path.c_str(), index, value, create);

      if (status != DB_SUCCESS) {
         printf("RS: db_get_value_string status %d\n", status);
      }
   }

   void WB(const char* varname, bool v)
   {
      std::string path;
      path += fRoot;
      path += "/";
      path += varname;
   
      LOCK_ODB();

      if (fTrace) {
         printf("Write ODB %s : %d\n", path.c_str(), v);
      }

      int status = db_set_value(fDB, 0, path.c_str(), &v, sizeof(int), 1, TID_BOOL);
      if (status != DB_SUCCESS) {
         printf("WB: db_set_value status %d\n", status);
      }
   }

   void WI(const char* varname, int v)
   {
      std::string path;
      path += fRoot;
      path += "/";
      path += varname;
   
      LOCK_ODB();

      if (fTrace) {
         printf("Write ODB %s : %d\n", path.c_str(), v);
      }

      int status = db_set_value(fDB, 0, path.c_str(), &v, sizeof(int), 1, TID_INT);
      if (status != DB_SUCCESS) {
         printf("WI: db_set_value status %d\n", status);
      }
   }

   void WD(const char* varname, double v)
   {
      std::string path;
      path += fRoot;
      path += "/";
      path += varname;
   
      LOCK_ODB();

      if (fTrace) {
         printf("Write ODB %s : %f\n", path.c_str(), v);
      }

      int status = db_set_value(fDB, 0, path.c_str(), &v, sizeof(double), 1, TID_DOUBLE);
      if (status != DB_SUCCESS) {
         printf("WD: db_set_value status %d\n", status);
      }
   }

   void WBA(const char* varname, const std::vector<bool>& v)
   {
      std::string path;
      path += fRoot;
      path += "/";
      path += varname;  

      BOOL *bb = new BOOL[v.size()];
      for (unsigned i=0; i<v.size(); i++) {
         bb[i] = v[i];
      }
   
      LOCK_ODB();

      if (fTrace) {
         printf("Write ODB %s : int[%d]\n", path.c_str(), (int)v.size());
      }

      int status = db_set_value(fDB, 0, path.c_str(), bb, v.size()*sizeof(BOOL), v.size(), TID_BOOL);
      if (status != DB_SUCCESS) {
         printf("WIA: db_set_value status %d\n", status);
      }
   }

   void WIA(const char* varname, const std::vector<int>& v)
   {
      std::string path;
      path += fRoot;
      path += "/";
      path += varname;
   
      LOCK_ODB();

      if (fTrace) {
         printf("Write ODB %s : int[%d]\n", path.c_str(), (int)v.size());
      }

      int status = db_set_value(fDB, 0, path.c_str(), &v[0], v.size()*sizeof(int), v.size(), TID_INT);
      if (status != DB_SUCCESS) {
         printf("WIA: db_set_value status %d\n", status);
      }
   }

   void WDA(const char* varname, const std::vector<double>& v)
   {
      std::string path;
      path += fRoot;
      path += "/";
      path += varname;
   
      LOCK_ODB();

      if (fTrace) {
         printf("Write ODB %s : double[%d]\n", path.c_str(), (int)v.size());
      }

      int status = db_set_value(fDB, 0, path.c_str(), &v[0], v.size()*sizeof(double), v.size(), TID_DOUBLE);
      if (status != DB_SUCCESS) {
         printf("WDA: db_set_value status %d\n", status);
      }
   }

   void WS(const char* varname, const char* v)
   {
      std::string path;
      path += fRoot;
      path += "/";
      path += varname;
   
      LOCK_ODB();

      int len = strlen(v);

      if (fTrace) {
         printf("Write ODB %s : string[%d]\n", path.c_str(), len);
      }

      int status = db_set_value(fDB, 0, path.c_str(), v, len+1, 1, TID_STRING);
      if (status != DB_SUCCESS) {
         printf("WS: db_set_value status %d\n", status);
      }
   }

   void WSA(const char* varname, const std::vector<std::string>& v, int odb_string_size)
   {
      std::string path;
      path += fRoot;
      path += "/";
      path += varname;
   
      LOCK_ODB();

      if (fTrace) {
         printf("Write ODB %s : string array[%d] odb_string_size %d\n", path.c_str(), (int)v.size(), odb_string_size);
      }

      unsigned num = v.size();
      unsigned length = odb_string_size;

      char val[length*num];
      memset(val, 0, length*num);
      
      for (unsigned i=0; i<num; i++)
         strlcpy(val+length*i, v[i].c_str(), length);
      
      int status = db_set_value(fDB, 0, path.c_str(), val, num*length, num, TID_STRING);
      if (status != DB_SUCCESS) {
         printf("WSA: db_set_value status %d\n", status);
      }
   }
};

TMVOdb* MakeOdb(int dbhandle)
{
   return new TMOdb(dbhandle, "");
}

/* emacs
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 3
 * indent-tabs-mode: nil
 * End:
 */

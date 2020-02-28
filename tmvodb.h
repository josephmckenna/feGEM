/********************************************************************\

  Name:         tmvodb.h
  Created by:   K.Olchanski

  Contents:     Virtual ODB interface

\********************************************************************/

#ifndef INCLUDE_TMVODB_H
#define INCLUDE_TMVODB_H

#include <string>
#include <vector>
#include <stdint.h>

class TMVOdb
{
public:
   virtual TMVOdb* Chdir(const char* subdir, bool create) = 0;
   
   // read array information: number of elements and element size (string size for TID_STRING arrays)

   virtual void RAInfo(const char* varname, int *num_elements, int *element_size) = 0;

   // read individual variables or array elements

   virtual void RB(const char* varname, int index, bool   *value, bool create) = 0;
   virtual void RI(const char* varname, int index, int    *value, bool create) = 0;
   virtual void RD(const char* varname, int index, double *value, bool create) = 0;
   virtual void RS(const char* varname, int index, std::string *value, bool create) = 0;
   virtual void RU32(const char* varname, int index, uint32_t *value, bool create) = 0;

   // read whole arrays

   virtual void RBA(const char* varname, std::vector<bool> *value, bool create, int create_size) = 0;
   virtual void RIA(const char* varname, std::vector<int>  *value, bool create, int create_size) = 0;
   virtual void RDA(const char* varname, std::vector<double> *value, bool create, int create_size) = 0;
   virtual void RSA(const char* varname, std::vector<std::string> *value, bool create, int create_size, int string_size) = 0;

   // write individual variables

   virtual void WB(const char* varname, bool v) = 0;
   virtual void WI(const char* varname, int v) = 0;
   virtual void WD(const char* varname, double v) = 0;
   virtual void WS(const char* varname, const char* v) = 0;

   // write whole arrays

   virtual void WBA(const char* varname, const std::vector<bool>& v) = 0;
   virtual void WIA(const char* varname, const std::vector<int>& v) = 0;
   virtual void WDA(const char* varname, const std::vector<double>& v) = 0;
   virtual void WSA(const char* varname, const std::vector<std::string>& v, int odb_string_length) = 0;
};

TMVOdb* MakeNullOdb();
TMVOdb* MakeOdb(int dbhandle);

#endif

/* emacs
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 3
 * indent-tabs-mode: nil
 * End:
 */

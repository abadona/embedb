//////////////////////////////////////////////////////////////////////////////
//// This software module is developed by SciDM (Scientific Data Management) in 1998-2015
//// 
//// This program is free software; you can redistribute, reuse,
//// or modify it with no restriction, under the terms of the MIT License.
//// 
//// This program is distributed in the hope that it will be useful,
//// but WITHOUT ANY WARRANTY; without even the implied warranty of
//// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//// 
//// For any questions please contact Denis Kaznadzey at dkaznadzey@yahoo.com
//////////////////////////////////////////////////////////////////////////////

#ifndef __i64out_h__
#define __i64out_h__
#include <ostream>
#include "portability.h"

#if defined (_MSC_VER)
std::ostream& operator << (std::ostream&, long long ii);
std::ostream& operator << (std::ostream&, unsigned long long ii);
#endif


#endif

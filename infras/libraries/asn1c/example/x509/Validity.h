/*
 * Generated by asn1c-0.9.28 (http://lionet.info/asn1c)
 * From ASN.1 module "X509"
 * 	found in "x509.asn1"
 * 	`asn1c -fwide-types`
 */

#ifndef _Validity_H_
#define _Validity_H_

#include <asn_application.h>

/* Including external dependencies */
#include "DateTime.h"
#include <constr_SEQUENCE.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Validity */
typedef struct Validity {
  DateTime_t notBefore;
  DateTime_t notAfter;

  /* Context for parsing across buffer boundaries */
  asn_struct_ctx_t _asn_ctx;
} Validity_t;

/* Implementation */
extern asn_TYPE_descriptor_t asn_DEF_Validity;

#ifdef __cplusplus
}
#endif

#endif /* _Validity_H_ */
#include <asn_internal.h>

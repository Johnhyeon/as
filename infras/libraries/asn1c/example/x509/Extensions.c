/*
 * Generated by asn1c-0.9.28 (http://lionet.info/asn1c)
 * From ASN.1 module "X509"
 * 	found in "x509.asn1"
 * 	`asn1c -fwide-types`
 */

#include "Extensions.h"

static asn_TYPE_member_t asn_MBR_Extensions_1[] = {
  {ATF_POINTER, 0, 0, (ASN_TAG_CLASS_UNIVERSAL | (16 << 2)), 0, &asn_DEF_Extension,
   0, /* Defer constraints checking to the member type */
   0, /* PER is not compiled, use -gen-PER */
   0, ""},
};
static const ber_tlv_tag_t asn_DEF_Extensions_tags_1[] = {(ASN_TAG_CLASS_UNIVERSAL | (16 << 2))};
static asn_SET_OF_specifics_t asn_SPC_Extensions_specs_1 = {
  sizeof(struct Extensions), offsetof(struct Extensions, _asn_ctx),
  0, /* XER encoding is XMLDelimitedItemList */
};
asn_TYPE_descriptor_t asn_DEF_Extensions = {
  "Extensions",
  "Extensions",
  SEQUENCE_OF_free,
  SEQUENCE_OF_print,
  SEQUENCE_OF_constraint,
  SEQUENCE_OF_decode_ber,
  SEQUENCE_OF_encode_der,
  SEQUENCE_OF_decode_xer,
  SEQUENCE_OF_encode_xer,
  0,
  0, /* No PER support, use "-gen-PER" to enable */
  0, /* Use generic outmost tag fetcher */
  asn_DEF_Extensions_tags_1,
  sizeof(asn_DEF_Extensions_tags_1) / sizeof(asn_DEF_Extensions_tags_1[0]), /* 1 */
  asn_DEF_Extensions_tags_1,                                                /* Same as above */
  sizeof(asn_DEF_Extensions_tags_1) / sizeof(asn_DEF_Extensions_tags_1[0]), /* 1 */
  0, /* No PER visible constraints */
  asn_MBR_Extensions_1,
  1,                          /* Single element */
  &asn_SPC_Extensions_specs_1 /* Additional specs */
};

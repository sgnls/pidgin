/**
 * @file certificate.h Public-Key Certificate API
 * @ingroup core
 */

/*
 *
 * purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _PURPLE_CERTIFICATE_H
#define _PURPLE_CERTIFICATE_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


typedef enum
{
	PURPLE_CERTIFICATE_INVALID = 0,
	PURPLE_CERTIFICATE_VALID = 1
} PurpleCertificateVerificationStatus;

typedef struct _PurpleCertificate PurpleCertificate;
typedef struct _PurpleCertificateScheme PurpleCertificateScheme;
typedef struct _PurpleCertificateVerifier PurpleCertificateVerifier;
typedef struct _PurpleCertificateVerificationRequest PurpleCertificateVerificationRequest;

/**
 * Callback function for the results of a verification check
 * @param vrq      Request structure operated on
 * @param st       Status code
 * @param userdata User-defined data
 */
typedef void (*PurpleCertificateVerifiedCallback)
		(PurpleCertificateVerificationRequest *vrq,
		 PurpleCertificateVerificationStatus,
		 gpointer userdata);
							  
/** A certificate instance
 *
 *  An opaque data structure representing a single certificate under some
 *  CertificateScheme
 */
struct _PurpleCertificate
{
	/** Scheme this certificate is under */
	PurpleCertificateScheme * scheme;
	/** Opaque pointer to internal data */
	gpointer data;
};

/** A certificate type
 *
 *  A CertificateScheme must implement all of the fields in the structure,
 *  and register it using purple_certificate_register_scheme()
 *
 *  There may be only ONE CertificateScheme provided for each certificate
 *  type, as specified by the "name" field.
 */
struct _PurpleCertificateScheme
{
	/** Name of the certificate type
	 *  ex: "x509", "pgp", etc.
	 *  This must be globally unique - you may not register more than one
	 *  CertificateScheme of the same name at a time.
	 */
	gchar * name;

	/** User-friendly name for this type
	 *  ex: N_("X.509 Certificates")
	 *  When this is displayed anywhere, it should be i18ned
	 *  ex: _(scheme->name)
	 */
	gchar * fullname;

	/** Imports a certificate from a file
	 *
	 *  @param filename   File to import the certificate from
	 *  @return           Pointer to the newly allocated Certificate struct
	 *                    or NULL on failure.
	 */
	PurpleCertificate * (* import_certificate)(const gchar * filename);

	/** Destroys and frees a Certificate structure
	 *
	 *  Destroys a Certificate's internal data structures and calls
	 *  free(crt)
	 *
	 *  @param crt  Certificate instance to be destroyed. It WILL NOT be
	 *              destroyed if it is not of the correct
	 *              CertificateScheme. Can be NULL
	 */
	void (* destroy_certificate)(PurpleCertificate * crt);

	/**
	 * Retrieves the certificate public key fingerprint using SHA1
	 *
	 * @param crt   Certificate instance
	 * @return Binary representation of SHA1 hash - must be freed using
	 *         g_byte_array_free()
	 */
	GByteArray * (* get_fingerprint_sha1)(PurpleCertificate *crt);

	/**
	 * Reads "who the certificate is assigned to"
	 *
	 * For SSL X.509 certificates, this is something like
	 * "gmail.com" or "jabber.org"
	 *
	 * @param crt   Certificate instance
	 * @return Newly allocated string specifying "whose certificate this
	 *         is"
	 */
	gchar * (* get_certificate_subject)(PurpleCertificate *crt);

	/**
	 * Retrieves a unique certificate identifier
	 *
	 * @param crt   Certificate instance
	 * @return Newly allocated string that can be used to uniquely
	 *         identify the certificate.
	 */
	gchar * (* get_unique_id)(PurpleCertificate *crt);

	/**
	 * Retrieves a unique identifier for the certificate's issuer
	 *
	 * @param crt   Certificate instance
	 * @return Newly allocated string that can be used to uniquely
	 *         identify the issuer's certificate.
	 */
	gchar * (* get_issuer_unique_id)(PurpleCertificate *crt);

	
	/* TODO: Fill out this structure */
};

/** A set of operations used to provide logic for verifying a Certificate's
 *  authenticity.
 */
struct _PurpleCertificateVerifier
{
	/** Name of the Verifier - case insensitive */
	gchar *name;
	
	/** Name of the scheme this Verifier operates on
	 *
	 * The scheme will be looked up by name when a Request is generated
	 * using this Verifier
	 */
	gchar *scheme_name;
};

/** Structure for a single certificate request
 *
 *  Useful for keeping track of the state of a verification that involves
 *  several steps
 */
struct _PurpleCertificateVerificationRequest
{
	/** Reference to the verification logic used */
	PurpleCertificateVerifier *verifier;

	/**
	 * Name to check that the certificate is issued to
	 *
	 * For X.509 certificates, this is the Common Name
	 */
	gchar *subject_name;
	
	/** List of certificates in the chain to be verified (such as that returned by purple_ssl_get_peer_certificates )
	 *
	 * This is most relevant for X.509 certificates used in SSL sessions.
	 * The list order should be: certificate, issuer, issuer's issuer, etc.
	 */
	GList *cert_chain;
	
	/** Internal data used by the Verifier code */
	gpointer data;

	/** Function to call with the verification result */
	PurpleCertificateVerifiedCallback cb;
	/** Data to pass to the post-verification callback */
	gpointer cb_data;
};

/*****************************************************************************/
/** @name PurpleCertificate Subsystem API                                    */
/*****************************************************************************/
/*@{*/

/** Look up a registered CertificateScheme by name
 * @param name   The scheme name. Case insensitive.
 * @return Pointer to the located Scheme, or NULL if it isn't found.
 */
PurpleCertificateScheme *
purple_certificate_find_scheme(const gchar *name);

/** Register a CertificateScheme with libpurple
 *
 * No two schemes can be registered with the same name; this function enforces
 * that.
 *
 * @param scheme  Pointer to the scheme to register.
 * @return TRUE if the scheme was successfully added, otherwise FALSE
 */
gboolean
purple_certificate_register_scheme(PurpleCertificateScheme *scheme);

/** Unregister a CertificateScheme from libpurple
 *
 * @param scheme    Scheme to unregister.
 *                  If the scheme is not registered, this is a no-op.
 *
 * @return TRUE if the unregister completed successfully
 */
gboolean
purple_certificate_unregister_scheme(PurpleCertificateScheme *scheme);

/* TODO: ADD STUFF HERE */

/*@}*/


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _PURPLE_CERTIFICATE_H */

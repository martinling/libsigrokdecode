/*
 * This file is part of the libsigrokdecode project.
 *
 * Copyright (C) 2010 Uwe Hermann <uwe@hermann-uwe.de>
 * Copyright (C) 2012 Bert Vermeulen <bert@biot.com>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef LIBSIGROKDECODE_SIGROKDECODE_H
#define LIBSIGROKDECODE_SIGROKDECODE_H

#include <Python.h> /* First, so we avoid a _POSIX_C_SOURCE warning. */
#include <stdint.h>
#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

struct srd_session;

/**
 * @file
 *
 * The public libsigrokdecode header file to be used by frontends.
 *
 * This is the only file that libsigrokdecode users (frontends) are supposed
 * to use and include. There are other header files which get installed with
 * libsigrokdecode, but those are not meant to be used directly by frontends.
 *
 * The correct way to get/use the libsigrokdecode API functions is:
 *
 * @code{.c}
 *   #include <libsigrokdecode/libsigrokdecode.h>
 * @endcode
 */

/*
 * All possible return codes of libsigrokdecode functions must be listed here.
 * Functions should never return hardcoded numbers as status, but rather
 * use these enum values. All error codes are negative numbers.
 *
 * The error codes are globally unique in libsigrokdecode, i.e. if one of the
 * libsigrokdecode functions returns a "malloc error" it must be exactly the
 * same return value as used by all other functions to indicate "malloc error".
 * There must be no functions which indicate two different errors via the
 * same return code.
 *
 * Also, for compatibility reasons, no defined return codes are ever removed
 * or reused for different errors later. You can only add new entries and
 * return codes, but never remove or redefine existing ones.
 */

/** Status/error codes returned by libsigrokdecode functions. */
enum {
	SRD_OK               =  0, /**< No error */
	SRD_ERR              = -1, /**< Generic/unspecified error */
	SRD_ERR_MALLOC       = -2, /**< Malloc/calloc/realloc error */
	SRD_ERR_ARG          = -3, /**< Function argument error */
	SRD_ERR_BUG          = -4, /**< Errors hinting at internal bugs */
	SRD_ERR_PYTHON       = -5, /**< Python C API error */
	SRD_ERR_DECODERS_DIR = -6, /**< Protocol decoder path invalid */

	/*
	 * Note: When adding entries here, don't forget to also update the
	 * srd_strerror() and srd_strerror_name() functions in error.c.
	 */
};

/* libsigrokdecode loglevels. */
enum {
	SRD_LOG_NONE = 0, /**< Output no messages at all. */
	SRD_LOG_ERR  = 1, /**< Output error messages. */
	SRD_LOG_WARN = 2, /**< Output warnings. */
	SRD_LOG_INFO = 3, /**< Output informational messages. */
	SRD_LOG_DBG  = 4, /**< Output debug messages. */
	SRD_LOG_SPEW = 5, /**< Output very noisy debug messages. */
};

/*
 * Use SRD_API to mark public API symbols, and SRD_PRIV for private symbols.
 *
 * Variables and functions marked 'static' are private already and don't
 * need SRD_PRIV. However, functions which are not static (because they need
 * to be used in other libsigrokdecode-internal files) but are also not
 * meant to be part of the public libsigrokdecode API, must use SRD_PRIV.
 *
 * This uses the 'visibility' feature of gcc (requires gcc >= 4.0).
 *
 * This feature is not available on MinGW/Windows, as it is a feature of
 * ELF files and MinGW/Windows uses PE files.
 *
 * Details: http://gcc.gnu.org/wiki/Visibility
 */

/* Marks public libsigrokdecode API symbols. */
#ifndef _WIN32
#define SRD_API __attribute__((visibility("default")))
#else
#define SRD_API
#endif

/* Marks private, non-public libsigrokdecode symbols (not part of the API). */
#ifndef _WIN32
#define SRD_PRIV __attribute__((visibility("hidden")))
#else
#define SRD_PRIV
#endif

/*
 * When adding an output type, don't forget to...
 *   - expose it to PDs in controller.c:PyInit_sigrokdecode()
 *   - add a check in module_sigrokdecode.c:Decoder_put()
 *   - add a debug string in type_decoder.c:OUTPUT_TYPES
 */
enum {
	SRD_OUTPUT_ANN,
	SRD_OUTPUT_PYTHON,
	SRD_OUTPUT_BINARY,
	SRD_OUTPUT_META,
};

enum {
	SRD_CONF_SAMPLERATE = 10000,
};

struct srd_decoder {
	/** The decoder ID. Must be non-NULL and unique for all decoders. */
	char *id;

	/** The (short) decoder name. Must be non-NULL. */
	char *name;

	/** The (long) decoder name. Must be non-NULL. */
	char *longname;

	/** A (short, one-line) description of the decoder. Must be non-NULL. */
	char *desc;

	/**
	 * The license of the decoder. Valid values: "gplv2+", "gplv3+".
	 * Other values are currently not allowed. Must be non-NULL.
	 */
	char *license;

	/** List of probes required by this decoder. */
	GSList *probes;

	/** List of optional probes for this decoder. */
	GSList *opt_probes;

	/**
	 * List of NULL-terminated char[], containing descriptions of the
	 * supported annotation output.
	 */
	GSList *annotations;

	/**
	 * List of NULL-terminated char[], containing descriptions of the
	 * supported binary output.
	 */
	GSList *binary;

	/** List of decoder options.  */
	GSList *options;

	/** Python module. */
	PyObject *py_mod;

	/** sigrokdecode.Decoder class. */
	PyObject *py_dec;
};

/**
 * Structure which contains information about one protocol decoder probe.
 * For example, I2C has two probes, SDA and SCL.
 */
struct srd_probe {
	/** The ID of the probe. Must be non-NULL. */
	char *id;
	/** The name of the probe. Must not be NULL. */
	char *name;
	/** The description of the probe. Must not be NULL. */
	char *desc;
	/** The index of the probe, i.e. its order in the list of probes. */
	int order;
};

struct srd_decoder_option {
	char *id;
	char *desc;
	GVariant *def;
};

struct srd_decoder_inst {
	struct srd_decoder *decoder;
	struct srd_session *sess;
	PyObject *py_inst;
	char *inst_id;
	GSList *pd_output;
	int dec_num_probes;
	int *dec_probemap;
	int data_unitsize;
	uint8_t *probe_samples;
	GSList *next_di;
};

struct srd_pd_output {
	int pdo_id;
	int output_type;
	struct srd_decoder_inst *di;
	char *proto_id;
	/* Only used for OUTPUT_META. */
	const GVariantType *meta_type;
	char *meta_name;
	char *meta_descr;
};

struct srd_proto_data {
	uint64_t start_sample;
	uint64_t end_sample;
	struct srd_pd_output *pdo;
	void *data;
};
struct srd_proto_data_annotation {
	int ann_format;
	char **ann_text;
};
struct srd_proto_data_binary {
	int bin_class;
	uint64_t size;
	const unsigned char *data;
};

typedef void (*srd_pd_output_callback_t)(struct srd_proto_data *pdata,
					 void *cb_data);

struct srd_pd_callback {
	int output_type;
	srd_pd_output_callback_t cb;
	void *cb_data;
};

/* Custom Python types: */

typedef struct {
	PyObject_HEAD
} srd_Decoder;

typedef struct {
	PyObject_HEAD
	struct srd_decoder_inst *di;
	uint64_t start_samplenum;
	unsigned int itercnt;
	uint8_t *inbuf;
	uint64_t inbuflen;
	PyObject *sample;
} srd_logic;


/* srd.c */
SRD_API int srd_init(const char *path);
SRD_API int srd_exit(void);

/* session.c */
SRD_API int srd_session_new(struct srd_session **sess);
SRD_API int srd_session_start(struct srd_session *sess);
SRD_API int srd_session_metadata_set(struct srd_session *sess, int key,
		GVariant *data);
SRD_API int srd_session_send(struct srd_session *sess,
		uint64_t start_samplenum, uint64_t end_samplenum,
		const uint8_t *inbuf, uint64_t inbuflen);
SRD_API int srd_session_destroy(struct srd_session *sess);
SRD_API int srd_pd_output_callback_add(struct srd_session *sess,
		int output_type, srd_pd_output_callback_t cb, void *cb_data);

/* decoder.c */
SRD_API const GSList *srd_decoder_list(void);
SRD_API struct srd_decoder *srd_decoder_get_by_id(const char *id);
SRD_API int srd_decoder_load(const char *name);
SRD_API char *srd_decoder_doc_get(const struct srd_decoder *dec);
SRD_API int srd_decoder_unload(struct srd_decoder *dec);
SRD_API int srd_decoder_load_all(void);
SRD_API int srd_decoder_unload_all(void);

/* instance.c */
SRD_API int srd_inst_option_set(struct srd_decoder_inst *di,
		GHashTable *options);
SRD_API int srd_inst_probe_set_all(struct srd_decoder_inst *di,
		GHashTable *probes);
SRD_API struct srd_decoder_inst *srd_inst_new(struct srd_session *sess,
		const char *id, GHashTable *options);
SRD_API int srd_inst_stack(struct srd_session *sess,
		struct srd_decoder_inst *di_from, struct srd_decoder_inst *di_to);
SRD_API struct srd_decoder_inst *srd_inst_find_by_id(struct srd_session *sess,
		const char *inst_id);

/* log.c */
typedef int (*srd_log_callback_t)(void *cb_data, int loglevel,
				  const char *format, va_list args);
SRD_API int srd_log_loglevel_set(int loglevel);
SRD_API int srd_log_loglevel_get(void);
SRD_API int srd_log_callback_set(srd_log_callback_t cb, void *cb_data);
SRD_API int srd_log_callback_set_default(void);
SRD_API int srd_log_logdomain_set(const char *logdomain);
SRD_API char *srd_log_logdomain_get(void);

/* error.c */
SRD_API const char *srd_strerror(int error_code);
SRD_API const char *srd_strerror_name(int error_code);

/* version.c */
SRD_API int srd_package_version_major_get(void);
SRD_API int srd_package_version_minor_get(void);
SRD_API int srd_package_version_micro_get(void);
SRD_API const char *srd_package_version_string_get(void);
SRD_API int srd_lib_version_current_get(void);
SRD_API int srd_lib_version_revision_get(void);
SRD_API int srd_lib_version_age_get(void);
SRD_API const char *srd_lib_version_string_get(void);

#include "version.h"

#ifdef __cplusplus
}
#endif

#endif

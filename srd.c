/*
 * This file is part of the libsigrokdecode project.
 *
 * Copyright (C) 2010 Uwe Hermann <uwe@hermann-uwe.de>
 * Copyright (C) 2013 Bert Vermeulen <bert@biot.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "libsigrokdecode.h" /* First, so we avoid a _POSIX_C_SOURCE warning. */
#include "libsigrokdecode-internal.h"
#include "config.h"
#include <glib.h>

/** @cond PRIVATE */

/* session.c */
extern GSList *sessions;
extern int max_session_id;

/* decoder.c */
extern SRD_PRIV GSList *pd_list;

/* module_sigrokdecode.c */
extern PyMODINIT_FUNC PyInit_sigrokdecode(void);

/** @endcond */

/**
 * @mainpage libsigrokdecode API
 *
 * @section sec_intro Introduction
 *
 * The <a href="http://sigrok.org">sigrok</a> project aims at creating a
 * portable, cross-platform, Free/Libre/Open-Source signal analysis software
 * suite that supports various device types (such as logic analyzers,
 * oscilloscopes, multimeters, and more).
 *
 * <a href="http://sigrok.org/wiki/Libsigrokdecode">libsigrokdecode</a> is a
 * shared library written in C which provides the basic API for (streaming)
 * protocol decoding functionality.
 *
 * The <a href="http://sigrok.org/wiki/Protocol_decoders">protocol decoders</a>
 * are written in Python (>= 3.0).
 *
 * @section sec_api API reference
 *
 * See the "Modules" page for an introduction to various libsigrokdecode
 * related topics and the detailed API documentation of the respective
 * functions.
 *
 * You can also browse the API documentation by file, or review all
 * data structures.
 *
 * @section sec_mailinglists Mailing lists
 *
 * There are two mailing lists for sigrok/libsigrokdecode: <a href="https://lists.sourceforge.net/lists/listinfo/sigrok-devel">sigrok-devel</a> and <a href="https://lists.sourceforge.net/lists/listinfo/sigrok-commits">sigrok-commits</a>.
 *
 * @section sec_irc IRC
 *
 * You can find the sigrok developers in the
 * <a href="irc://chat.freenode.net/sigrok">\#sigrok</a>
 * IRC channel on Freenode.
 *
 * @section sec_website Website
 *
 * <a href="http://sigrok.org/wiki/Libsigrokdecode">sigrok.org/wiki/Libsigrokdecode</a>
 */

/**
 * @file
 *
 * Initializing and shutting down libsigrokdecode.
 */

/**
 * @defgroup grp_init Initialization
 *
 * Initializing and shutting down libsigrokdecode.
 *
 * Before using any of the libsigrokdecode functionality, srd_init() must
 * be called to initialize the library.
 *
 * When libsigrokdecode functionality is no longer needed, srd_exit() should
 * be called.
 *
 * @{
 */

/**
 * Initialize libsigrokdecode.
 *
 * This initializes the Python interpreter, and creates and initializes
 * a "sigrokdecode" Python module.
 *
 * Then, it searches for sigrok protocol decoders in the "decoders"
 * subdirectory of the the libsigrokdecode installation directory.
 * All decoders that are found are loaded into memory and added to an
 * internal list of decoders, which can be queried via srd_decoder_list().
 *
 * The caller is responsible for calling the clean-up function srd_exit(),
 * which will properly shut down libsigrokdecode and free its allocated memory.
 *
 * Multiple calls to srd_init(), without calling srd_exit() in between,
 * are not allowed.
 *
 * @param path Path to an extra directory containing protocol decoders
 *             which will be added to the Python sys.path. May be NULL.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *         Upon Python errors, SRD_ERR_PYTHON is returned. If the decoders
 *         directory cannot be accessed, SRD_ERR_DECODERS_DIR is returned.
 *         If not enough memory could be allocated, SRD_ERR_MALLOC is returned.
 *
 * @since 0.1.0
 */
SRD_API int srd_init(const char *path)
{
	int ret;
	char *env_path;

	if (max_session_id != -1) {
		srd_err("libsigrokdecode is already initialized.");
		return SRD_ERR;
	}

	srd_dbg("Initializing libsigrokdecode.");

	/* Add our own module to the list of built-in modules. */
	PyImport_AppendInittab("sigrokdecode", PyInit_sigrokdecode);

	/* Initialize the Python interpreter. */
	Py_Initialize();

	/* Installed decoders. */
	if ((ret = srd_decoder_searchpath_add(DECODERS_DIR)) != SRD_OK) {
		Py_Finalize();
		return ret;
	}

	/* Path specified by the user. */
	if (path) {
		if ((ret = srd_decoder_searchpath_add(path)) != SRD_OK) {
			Py_Finalize();
			return ret;
		}
	}

	/* Environment variable overrides everything, for debugging. */
	if ((env_path = getenv("SIGROKDECODE_DIR"))) {
		if ((ret = srd_decoder_searchpath_add(env_path)) != SRD_OK) {
			Py_Finalize();
			return ret;
		}
	}

	max_session_id = 0;

	return SRD_OK;
}

/**
 * Shutdown libsigrokdecode.
 *
 * This frees all the memory allocated for protocol decoders and shuts down
 * the Python interpreter.
 *
 * This function should only be called if there was a (successful!) invocation
 * of srd_init() before. Calling this function multiple times in a row, without
 * any successful srd_init() calls in between, is not allowed.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *
 * @since 0.1.0
 */
SRD_API int srd_exit(void)
{
	GSList *l;

	srd_dbg("Exiting libsigrokdecode.");

	for (l = sessions; l; l = l->next)
		srd_session_destroy((struct srd_session *)l->data);

	srd_decoder_unload_all();
	g_slist_free(pd_list);
	pd_list = NULL;

	/* Py_Finalize() returns void, any finalization errors are ignored. */
	Py_Finalize();

	max_session_id = -1;

	return SRD_OK;
}

/**
 * Add an additional search directory for the protocol decoders.
 *
 * The specified directory is prepended (not appended!) to Python's sys.path,
 * in order to search for sigrok protocol decoders in the specified
 * directories first, and in the generic Python module directories (and in
 * the current working directory) last. This avoids conflicts if there are
 * Python modules which have the same name as a sigrok protocol decoder in
 * sys.path or in the current working directory.
 *
 * @param path Path to the directory containing protocol decoders which shall
 *             be added to the Python sys.path, or NULL.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *
 * @private
 *
 * @since 0.1.0
 */
SRD_PRIV int srd_decoder_searchpath_add(const char *path)
{
	PyObject *py_cur_path, *py_item;
	GString *new_path;
	int wc_len, i;
	wchar_t *wc_new_path;
	char *item;

	srd_dbg("Adding '%s' to module path.", path);

	new_path = g_string_sized_new(256);
	g_string_assign(new_path, path);
	py_cur_path = PySys_GetObject("path");
	for (i = 0; i < PyList_Size(py_cur_path); i++) {
		g_string_append(new_path, G_SEARCHPATH_SEPARATOR_S);
		py_item = PyList_GetItem(py_cur_path, i);
		if (!PyUnicode_Check(py_item))
			/* Shouldn't happen. */
			continue;
		if (py_str_as_str(py_item, &item) != SRD_OK)
			continue;
		g_string_append(new_path, item);
		g_free(item);
	}

	/* Convert to wide chars. */
	wc_len = sizeof(wchar_t) * (new_path->len + 1);
	if (!(wc_new_path = g_try_malloc(wc_len))) {
		srd_dbg("malloc failed");
		return SRD_ERR_MALLOC;
	}
	mbstowcs(wc_new_path, new_path->str, wc_len);
	PySys_SetPath(wc_new_path);
	g_string_free(new_path, TRUE);
	g_free(wc_new_path);

	return SRD_OK;
}

/* @private */
SRD_PRIV gboolean srd_check_init(void)
{
	if (max_session_id < 0) {
		srd_err("Library is not initialized.");
		return FALSE;
	} else
		return TRUE;
}

/** @} */

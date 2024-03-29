/*
 * This file is part of the libsigrokdecode project.
 *
 * Copyright (C) 2010 Uwe Hermann <uwe@hermann-uwe.de>
 * Copyright (C) 2012 Bert Vermeulen <bert@biot.com>
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
#include <inttypes.h>
#include <stdlib.h>
#include <stdint.h>

/** @cond PRIVATE */

extern GSList *sessions;

/* type_logic.c */
extern SRD_PRIV PyTypeObject srd_logic_type;

/** @endcond */

/**
 * @file
 *
 * Decoder instance handling.
 */

/**
 * @defgroup grp_instances Decoder instances
 *
 * Decoder instance handling.
 *
 * @{
 */

/**
 * Set one or more options in a decoder instance.
 *
 * Handled options are removed from the hash.
 *
 * @param di Decoder instance.
 * @param options A GHashTable of options to set.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *
 * @since 0.1.0
 */
SRD_API int srd_inst_option_set(struct srd_decoder_inst *di,
		GHashTable *options)
{
	PyObject *py_dec_options, *py_dec_optkeys, *py_di_options, *py_optval;
	PyObject *py_optlist, *py_classval;
	Py_UNICODE *py_ustr;
	GVariant *value;
	unsigned long long int val_ull;
	gint64 val_int;
	int num_optkeys, ret, size, i;
	const char *val_str;
	char *dbg, *key;

	if (!di) {
		srd_err("Invalid decoder instance.");
		return SRD_ERR_ARG;
	}

	if (!options) {
		srd_err("Invalid options GHashTable.");
		return SRD_ERR_ARG;
	}

	if (!PyObject_HasAttrString(di->decoder->py_dec, "options")) {
		/* Decoder has no options. */
		if (g_hash_table_size(options) == 0) {
			/* No options provided. */
			return SRD_OK;
		} else {
			srd_err("Protocol decoder has no options.");
			return SRD_ERR_ARG;
		}
		return SRD_OK;
	}

	ret = SRD_ERR_PYTHON;
	key = NULL;
	py_dec_options = py_dec_optkeys = py_di_options = py_optval = NULL;
	py_optlist = py_classval = NULL;
	py_dec_options = PyObject_GetAttrString(di->decoder->py_dec, "options");

	/* All of these are synthesized objects, so they're good. */
	py_dec_optkeys = PyDict_Keys(py_dec_options);
	num_optkeys = PyList_Size(py_dec_optkeys);

	/*
	 * The 'options' dictionary is a class variable, but we need to
	 * change it. Changing it directly will affect the entire class,
	 * so we need to create a new object for it, and populate that
	 * instead.
	 */
	if (!(py_di_options = PyObject_GetAttrString(di->py_inst, "options")))
		goto err_out;
	Py_DECREF(py_di_options);
	py_di_options = PyDict_New();
	PyObject_SetAttrString(di->py_inst, "options", py_di_options);
	for (i = 0; i < num_optkeys; i++) {
		/* Get the default class value for this option. */
		py_str_as_str(PyList_GetItem(py_dec_optkeys, i), &key);
		if (!(py_optlist = PyDict_GetItemString(py_dec_options, key)))
			goto err_out;
		if (!(py_classval = PyList_GetItem(py_optlist, 1)))
			goto err_out;
		if (!PyUnicode_Check(py_classval) && !PyLong_Check(py_classval)) {
			srd_err("Options of type %s are not yet supported.",
				Py_TYPE(py_classval)->tp_name);
			goto err_out;
		}

		if ((value = g_hash_table_lookup(options, key))) {
			dbg = g_variant_print(value, TRUE);
			srd_dbg("got option '%s' = %s", key, dbg);
			g_free(dbg);
			/* An override for this option was provided. */
			if (PyUnicode_Check(py_classval)) {
				if (!g_variant_is_of_type(value, G_VARIANT_TYPE_STRING)) {
					srd_err("Option '%s' requires a string value.", key);
					goto err_out;
				}
				val_str = g_variant_get_string(value, NULL);
				if (!(py_optval = PyUnicode_FromString(val_str))) {
					/* Some UTF-8 encoding error. */
					PyErr_Clear();
					srd_err("Option '%s' requires a UTF-8 string value.", key);
					goto err_out;
				}
			} else if (PyLong_Check(py_classval)) {
				if (!g_variant_is_of_type(value, G_VARIANT_TYPE_INT64)) {
					srd_err("Option '%s' requires an integer value.", key);
					goto err_out;
				}
				val_int = g_variant_get_int64(value);
				if (!(py_optval = PyLong_FromLong(val_int))) {
					/* ValueError Exception */
					PyErr_Clear();
					srd_err("Option '%s' has invalid integer value.", key);
					goto err_out;
				}
			}
			g_hash_table_remove(options, key);
		} else {
			/* Use the class default for this option. */
			if (PyUnicode_Check(py_classval)) {
				/* Make a brand new copy of the string. */
				py_ustr = PyUnicode_AS_UNICODE(py_classval);
				size = PyUnicode_GET_SIZE(py_classval);
				py_optval = PyUnicode_FromUnicode(py_ustr, size);
			} else if (PyLong_Check(py_classval)) {
				/* Make a brand new copy of the integer. */
				val_ull = PyLong_AsUnsignedLongLong(py_classval);
				if (val_ull == (unsigned long long)-1) {
					/* OverFlowError exception */
					PyErr_Clear();
					srd_err("Invalid integer value for %s: "
						"expected integer.", key);
					goto err_out;
				}
				if (!(py_optval = PyLong_FromUnsignedLongLong(val_ull)))
					goto err_out;
			}
		}

		/*
		 * If we got here, py_optval holds a known good new reference
		 * to the instance option to set.
		 */
		if (PyDict_SetItemString(py_di_options, key, py_optval) == -1)
			goto err_out;
		g_free(key);
		key = NULL;
	}

	ret = SRD_OK;

err_out:
	Py_XDECREF(py_di_options);
	Py_XDECREF(py_dec_optkeys);
	Py_XDECREF(py_dec_options);
	g_free(key);
	if (PyErr_Occurred()) {
		srd_exception_catch("Stray exception in srd_inst_option_set().");
		ret = SRD_ERR_PYTHON;
	}

	return ret;
}

/* Helper GComparefunc for g_slist_find_custom() in srd_inst_probe_set_all() */
static gint compare_probe_id(const struct srd_probe *a, const char *probe_id)
{
	return strcmp(a->id, probe_id);
}

/**
 * Set all probes in a decoder instance.
 *
 * This function sets _all_ probes for the specified decoder instance, i.e.,
 * it overwrites any probes that were already defined (if any).
 *
 * @param di Decoder instance.
 * @param new_probes A GHashTable of probes to set. Key is probe name, value is
 *                   the probe number. Samples passed to this instance will be
 *                   arranged in this order.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *
 * @since 0.1.0
 */
SRD_API int srd_inst_probe_set_all(struct srd_decoder_inst *di,
		GHashTable *new_probes)
{
	GVariant *probe_val;
	GList *l;
	GSList *sl;
	struct srd_probe *p;
	int *new_probemap, new_probenum, num_required_probes, num_probes, i;
	char *probe_id;

	srd_dbg("set probes called for instance %s with list of %d probes",
		di->inst_id, g_hash_table_size(new_probes));

	if (g_hash_table_size(new_probes) == 0)
		/* No probes provided. */
		return SRD_OK;

	if (di->dec_num_probes == 0) {
		/* Decoder has no probes. */
		srd_err("Protocol decoder %s has no probes to define.",
			di->decoder->name);
		return SRD_ERR_ARG;
	}

	new_probemap = NULL;

	if (!(new_probemap = g_try_malloc(sizeof(int) * di->dec_num_probes))) {
		srd_err("Failed to g_malloc() new probe map.");
		return SRD_ERR_MALLOC;
	}

	/*
	 * For now, map all indexes to probe -1 (can be overridden later).
	 * This -1 is interpreted as an unspecified probe later.
	 */
	for (i = 0; i < di->dec_num_probes; i++)
		new_probemap[i] = -1;

	num_probes = 0;
	for (l = g_hash_table_get_keys(new_probes); l; l = l->next) {
		probe_id = l->data;
		probe_val = g_hash_table_lookup(new_probes, probe_id);
		if (!g_variant_is_of_type(probe_val, G_VARIANT_TYPE_INT32)) {
			/* Probe name was specified without a value. */
			srd_err("No probe number was specified for %s.",
					probe_id);
			g_free(new_probemap);
			return SRD_ERR_ARG;
		}
		new_probenum = g_variant_get_int32(probe_val);
		if (!(sl = g_slist_find_custom(di->decoder->probes, probe_id,
				(GCompareFunc)compare_probe_id))) {
			/* Fall back on optional probes. */
			if (!(sl = g_slist_find_custom(di->decoder->opt_probes,
			     probe_id, (GCompareFunc) compare_probe_id))) {
				srd_err("Protocol decoder %s has no probe "
						"'%s'.", di->decoder->name, probe_id);
				g_free(new_probemap);
				return SRD_ERR_ARG;
			}
		}
		p = sl->data;
		new_probemap[p->order] = new_probenum;
		srd_dbg("Setting probe mapping: %s (index %d) = probe %d.",
			p->id, p->order, new_probenum);
		num_probes++;
	}
	di->data_unitsize = (num_probes + 7) / 8;

	srd_dbg("Final probe map:");
	num_required_probes = g_slist_length(di->decoder->probes);
	for (i = 0; i < di->dec_num_probes; i++) {
		srd_dbg(" - index %d = probe %d (%s)", i, new_probemap[i],
		        (i < num_required_probes) ? "required" : "optional");
	}

	g_free(di->dec_probemap);
	di->dec_probemap = new_probemap;

	return SRD_OK;
}

/**
 * Create a new protocol decoder instance.
 *
 * @param sess The session holding the protocol decoder instance.
 * @param decoder_id Decoder 'id' field.
 * @param options GHashtable of options which override the defaults set in
 *                the decoder class. May be NULL.
 *
 * @return Pointer to a newly allocated struct srd_decoder_inst, or
 *         NULL in case of failure.
 *
 * @since 0.3.0
 */
SRD_API struct srd_decoder_inst *srd_inst_new(struct srd_session *sess,
		const char *decoder_id, GHashTable *options)
{
	int i;
	struct srd_decoder *dec;
	struct srd_decoder_inst *di;
	char *inst_id;

	srd_dbg("Creating new %s instance.", decoder_id);

	if (session_is_valid(sess) != SRD_OK) {
		srd_err("Invalid session.");
		return NULL;
	}

	if (!(dec = srd_decoder_get_by_id(decoder_id))) {
		srd_err("Protocol decoder %s not found.", decoder_id);
		return NULL;
	}

	if (!(di = g_try_malloc0(sizeof(struct srd_decoder_inst)))) {
		srd_err("Failed to g_malloc() instance.");
		return NULL;
	}

	di->decoder = dec;
	di->sess = sess;
	if (options) {
		inst_id = g_hash_table_lookup(options, "id");
		di->inst_id = g_strdup(inst_id ? inst_id : decoder_id);
		g_hash_table_remove(options, "id");
	} else
		di->inst_id = g_strdup(decoder_id);

	/*
	 * Prepare a default probe map, where samples come in the
	 * order in which the decoder class defined them.
	 */
	di->dec_num_probes = g_slist_length(di->decoder->probes) +
			g_slist_length(di->decoder->opt_probes);
	if (di->dec_num_probes) {
		if (!(di->dec_probemap =
				g_try_malloc(sizeof(int) * di->dec_num_probes))) {
			srd_err("Failed to g_malloc() probe map.");
			g_free(di);
			return NULL;
		}
		for (i = 0; i < di->dec_num_probes; i++)
			di->dec_probemap[i] = i;
		di->data_unitsize = (di->dec_num_probes + 7) / 8;
		/*
		 * Will be used to prepare a sample at every iteration
		 * of the instance's decode() method.
		 */
		if (!(di->probe_samples = g_try_malloc(di->dec_num_probes))) {
			srd_err("Failed to g_malloc() sample buffer.");
			g_free(di->dec_probemap);
			g_free(di);
			return NULL;
		}
	}

	/* Create a new instance of this decoder class. */
	if (!(di->py_inst = PyObject_CallObject(dec->py_dec, NULL))) {
		if (PyErr_Occurred())
			srd_exception_catch("failed to create %s instance: ",
					decoder_id);
		g_free(di->dec_probemap);
		g_free(di);
		return NULL;
	}

	if (options && srd_inst_option_set(di, options) != SRD_OK) {
		g_free(di->dec_probemap);
		g_free(di);
		return NULL;
	}

	/* Instance takes input from a frontend by default. */
	sess->di_list = g_slist_append(sess->di_list, di);

	return di;
}

/**
 * Stack a decoder instance on top of another.
 *
 * @param sess The session holding the protocol decoder instances.
 * @param di_from The instance to move.
 * @param di_to The instance on top of which di_from will be stacked.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *
 * @since 0.3.0
 */
SRD_API int srd_inst_stack(struct srd_session *sess,
		struct srd_decoder_inst *di_from, struct srd_decoder_inst *di_to)
{

	if (session_is_valid(sess) != SRD_OK) {
		srd_err("Invalid session.");
		return SRD_ERR_ARG;
	}

	if (!di_from || !di_to) {
		srd_err("Invalid from/to instance pair.");
		return SRD_ERR_ARG;
	}

	if (g_slist_find(sess->di_list, di_to)) {
		/* Remove from the unstacked list. */
		sess->di_list = g_slist_remove(sess->di_list, di_to);
	}

	/* Stack on top of source di. */
	di_from->next_di = g_slist_append(di_from->next_di, di_to);

	return SRD_OK;
}

/**
 * Find a decoder instance by its instance ID.
 *
 * Only the bottom level of instances are searched -- instances already stacked
 * on top of another one will not be found.
 *
 * @param sess The session holding the protocol decoder instance.
 * @param inst_id The instance ID to be found.
 *
 * @return Pointer to struct srd_decoder_inst, or NULL if not found.
 *
 * @since 0.3.0
 */
SRD_API struct srd_decoder_inst *srd_inst_find_by_id(struct srd_session *sess,
		const char *inst_id)
{
	GSList *l;
	struct srd_decoder_inst *tmp, *di;

	if (session_is_valid(sess) != SRD_OK) {
		srd_err("Invalid session.");
		return NULL;
	}

	di = NULL;
	for (l = sess->di_list; l; l = l->next) {
		tmp = l->data;
		if (!strcmp(tmp->inst_id, inst_id)) {
			di = tmp;
			break;
		}
	}

	return di;
}

static struct srd_decoder_inst *srd_sess_inst_find_by_obj(
		struct srd_session *sess, const GSList *stack,
		const PyObject *obj)
{
	const GSList *l;
	struct srd_decoder_inst *tmp, *di;

	if (session_is_valid(sess) != SRD_OK) {
		srd_err("Invalid session.");
		return NULL;
	}

	di = NULL;
	for (l = stack ? stack : sess->di_list; di == NULL && l != NULL; l = l->next) {
		tmp = l->data;
		if (tmp->py_inst == obj)
			di = tmp;
		else if (tmp->next_di)
			di = srd_sess_inst_find_by_obj(sess, tmp->next_di, obj);
	}

	return di;
}

/**
 * Find a decoder instance by its Python object.
 *
 * I.e. find that instance's instantiation of the sigrokdecode.Decoder class.
 * This will recurse to find the instance anywhere in the stack tree of all
 * sessions.
 *
 * @param stack Pointer to a GSList of struct srd_decoder_inst, indicating the
 *              stack to search. To start searching at the bottom level of
 *              decoder instances, pass NULL.
 * @param obj The Python class instantiation.
 *
 * @return Pointer to struct srd_decoder_inst, or NULL if not found.
 *
 * @private
 *
 * @since 0.1.0
 */
SRD_PRIV struct srd_decoder_inst *srd_inst_find_by_obj(const GSList *stack,
		const PyObject *obj)
{
	struct srd_decoder_inst *di;
	struct srd_session *sess;
	GSList *l;

	di = NULL;
	for (l = sessions; di == NULL && l != NULL; l = l->next) {
		sess = l->data;
		di = srd_sess_inst_find_by_obj(sess, stack, obj);
	}

	return di;
}

/** @private */
SRD_PRIV int srd_inst_start(struct srd_decoder_inst *di)
{
	PyObject *py_res;
	GSList *l;
	struct srd_decoder_inst *next_di;
	int ret;

	srd_dbg("Calling start() method on protocol decoder instance %s.",
			di->inst_id);

	if (!(py_res = PyObject_CallMethod(di->py_inst, "start", NULL))) {
		srd_exception_catch("Protocol decoder instance %s: ",
				di->inst_id);
		return SRD_ERR_PYTHON;
	}
	Py_DecRef(py_res);

	/* Start all the PDs stacked on top of this one. */
	for (l = di->next_di; l; l = l->next) {
		next_di = l->data;
		if ((ret = srd_inst_start(next_di)) != SRD_OK)
			return ret;
	}

	return SRD_OK;
}

/**
 * Run the specified decoder function.
 *
 * @param di The decoder instance to call. Must not be NULL.
 * @param start_samplenum The starting sample number for the buffer's sample
 * 			  set, relative to the start of capture.
 * @param end_samplenum The ending sample number for the buffer's sample
 * 			  set, relative to the start of capture.
 * @param inbuf The buffer to decode. Must not be NULL.
 * @param inbuflen Length of the buffer. Must be > 0.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *
 * @private
 *
 * @since 0.1.0
 */
SRD_PRIV int srd_inst_decode(const struct srd_decoder_inst *di,
		uint64_t start_samplenum, uint64_t end_samplenum,
		const uint8_t *inbuf, uint64_t inbuflen)
{
	PyObject *py_res;
	srd_logic *logic;

	srd_dbg("Calling decode() on instance %s with %" PRIu64 " bytes "
		"starting at sample %" PRIu64 ".", di->inst_id, inbuflen,
		start_samplenum);

	/* Return an error upon unusable input. */
	if (!di) {
		srd_dbg("empty decoder instance");
		return SRD_ERR_ARG;
	}
	if (!inbuf) {
		srd_dbg("NULL buffer pointer");
		return SRD_ERR_ARG;
	}
	if (inbuflen == 0) {
		srd_dbg("empty buffer");
		return SRD_ERR_ARG;
	}

	/*
	 * Create new srd_logic object. Each iteration around the PD's loop
	 * will fill one sample into this object.
	 */
	logic = PyObject_New(srd_logic, &srd_logic_type);
	Py_INCREF(logic);
	logic->di = (struct srd_decoder_inst *)di;
	logic->start_samplenum = start_samplenum;
	logic->itercnt = 0;
	logic->inbuf = (uint8_t *)inbuf;
	logic->inbuflen = inbuflen;
	logic->sample = PyList_New(2);
	Py_INCREF(logic->sample);

	Py_IncRef(di->py_inst);
	if (!(py_res = PyObject_CallMethod(di->py_inst, "decode",
			"KKO", start_samplenum, end_samplenum, logic))) {
		srd_exception_catch("Protocol decoder instance %s: ", di->inst_id);
		return SRD_ERR_PYTHON;
	}
	Py_DecRef(py_res);

	return SRD_OK;
}

/** @private */
SRD_PRIV void srd_inst_free(struct srd_decoder_inst *di)
{
	GSList *l;
	struct srd_pd_output *pdo;

	srd_dbg("Freeing instance %s", di->inst_id);

	Py_DecRef(di->py_inst);
	g_free(di->inst_id);
	g_free(di->dec_probemap);
	g_slist_free(di->next_di);
	for (l = di->pd_output; l; l = l->next) {
		pdo = l->data;
		g_free(pdo->proto_id);
		g_free(pdo);
	}
	g_slist_free(di->pd_output);
	g_free(di);
}

/** @private */
SRD_PRIV void srd_inst_free_all(struct srd_session *sess, GSList *stack)
{
	GSList *l;
	struct srd_decoder_inst *di;

	if (session_is_valid(sess) != SRD_OK) {
		srd_err("Invalid session.");
		return;
	}

	di = NULL;
	for (l = stack ? stack : sess->di_list; di == NULL && l != NULL; l = l->next) {
		di = l->data;
		if (di->next_di)
			srd_inst_free_all(sess, di->next_di);
		srd_inst_free(di);
	}
	if (!stack) {
		g_slist_free(sess->di_list);
		sess->di_list = NULL;
	}
}

/** @} */


#include <uwsgi.h>

#include "php.h"
#include "SAPI.h"
#include "php_main.h"
#include "php_variables.h"

#include "ext/standard/php_smart_str.h"
#include "ext/standard/info.h"

static sapi_module_struct uwsgi_phpsgi_sapi_module;

static struct uwsgi_phpsgi {
	// the name of the function to call at every request
	zval function_name;
} uwsgi_phpsgi;


static int sapi_phpsgi_ub_write(const char *str, uint str_length TSRMLS_DC) {
	uwsgi_log("%.*s", str_length, (char *) str);
	return str_length;
}

static int sapi_uwsgi_send_headers(sapi_headers_struct *sapi_headers TSRMLS_DC) {
	return SAPI_HEADER_SENT_SUCCESSFULLY;
}

static char *sapi_uwsgi_read_cookies(TSRMLS_D) {
	return NULL;
}

static void sapi_uwsgi_register_variables(zval *track_vars_array TSRMLS_DC) {
}

static int php_uwsgi_startup(sapi_module_struct *sapi_module) {
	if (php_module_startup(&uwsgi_phpsgi_sapi_module, NULL, 0) == FAILURE) {
		return FAILURE;
	}
	return SUCCESS;
}

static void sapi_uwsgi_log_message(char *message TSRMLS_DC) {
	uwsgi_log("%s\n", message);
}

static sapi_module_struct uwsgi_phpsgi_sapi_module = {
	"uwsgi-phpsgi",
	"uWSGI/phpsgi",
	
	php_uwsgi_startup,
	php_module_shutdown_wrapper,
	
	NULL,									/* activate */
	NULL,									/* deactivate */

	sapi_phpsgi_ub_write,
	NULL,
	NULL,									/* get uid */
	NULL,									/* getenv */

	php_error,
	
	NULL,
	sapi_uwsgi_send_headers,
	NULL,
	NULL,
	sapi_uwsgi_read_cookies,

	sapi_uwsgi_register_variables,
	sapi_uwsgi_log_message,									/* Log message */
	NULL,									/* Get request time */
	NULL,									/* Child terminate */

	STANDARD_SAPI_MODULE_PROPERTIES
};

int uwsgi_phpsgi_init(void) {

#ifdef ZTS
        tsrm_startup(1, 1, 0, NULL);
#endif

	sapi_startup(&uwsgi_phpsgi_sapi_module);
	uwsgi_phpsgi_sapi_module.startup(&uwsgi_phpsgi_sapi_module);
	uwsgi_log("PHP %s initialized\n", PHP_VERSION);

	php_request_startup(TSRMLS_C);

	zend_file_handle file_handle;

	file_handle.type = ZEND_HANDLE_FP;
	file_handle.opened_path = NULL;
	file_handle.free_filename = 0;
	file_handle.filename = "app.php";
	file_handle.handle.fp = fopen(file_handle.filename, "r");

	php_output_end(TSRMLS_C);

	php_execute_script(&file_handle TSRMLS_CC);

	INIT_ZVAL(uwsgi_phpsgi.function_name);
	ZVAL_STRING(&uwsgi_phpsgi.function_name, "application", 1);

	return 0;
}

static int phpsgi_generate_header(struct wsgi_request *wsgi_req, HashTable *headers) {
	HashPosition pointer;
	zval **data;
	unsigned long index;
	for(zend_hash_internal_pointer_reset_ex(headers, &pointer); zend_hash_get_current_data_ex(headers, (void **) &data, &pointer) == SUCCESS;
		zend_hash_move_forward_ex(headers, &pointer)) {
		char *key;
    		unsigned int key_len;
		if (zend_hash_get_current_key_ex(headers, &key, &key_len, &index, 0, &pointer) != HASH_KEY_IS_STRING) return -1;
		uwsgi_log("HEADER %.*s\n", key_len, key);	
		if (Z_TYPE_PP(data) == IS_STRING) {
			if (uwsgi_response_add_header(wsgi_req, key, key_len, Z_STRVAL_PP(data), Z_STRLEN_PP(data))) return -1;
		}
		else if (Z_TYPE_PP(data) == IS_ARRAY) {
			int i, len = zend_hash_num_elements(Z_ARRVAL_PP(data));
			for(i=0;i<len;i++) {
				zval **hv;
				zend_hash_index_find(Z_ARRVAL_PP(data), i, (void **)&hv);
				if (Z_TYPE_PP(hv) != IS_STRING) return -1;
				if (uwsgi_response_add_header(wsgi_req, key, key_len, Z_STRVAL_PP(hv), Z_STRLEN_PP(hv))) return -1;
			}
		}
	}
	return 0;
}

int uwsgi_phpsgi_request(struct wsgi_request *wsgi_req) {

#ifdef ZTS
	TSRMLS_FETCH();
#endif

	if (uwsgi_parse_vars(wsgi_req)) {
		return -1;
	}

	// fill $env
	zval *env;	
	ALLOC_INIT_ZVAL(env);
	array_init(env);

	int i;
	for (i = 0; i < wsgi_req->var_cnt; i += 2) {
		// we need null-terminated keys
		char *key = uwsgi_concat2n(wsgi_req->hvec[i].iov_base, wsgi_req->hvec[i].iov_len, "", 0);
		add_assoc_stringl_ex(env, key, wsgi_req->hvec[i].iov_len+1, wsgi_req->hvec[i + 1].iov_base, wsgi_req->hvec[i + 1].iov_len, 1);
		free(key);
        }

        zval retval_ptr;

        int ret = call_user_function(CG(function_table), NULL, &uwsgi_phpsgi.function_name, &retval_ptr, 1, &env TSRMLS_CC);
	uwsgi_log("ret = %d\n", ret);
	if (ret != SUCCESS) goto error;

        if (Z_TYPE(retval_ptr) == IS_STRING) {
		if (uwsgi_response_prepare_headers(wsgi_req, "200 OK", 6)) goto error;
		uwsgi_response_write_body_do(wsgi_req, Z_STRVAL(retval_ptr), Z_STRLEN(retval_ptr));	
        }
        else if (Z_TYPE(retval_ptr) == IS_ARRAY) {
                if (zend_hash_num_elements(Z_ARRVAL(retval_ptr)) != 3) goto error;
                zval **zv_dest = NULL;

                zend_hash_index_find(Z_ARRVAL(retval_ptr), 0, (void **)&zv_dest);
                if (Z_TYPE_PP(zv_dest) != IS_STRING) goto error;
		if (uwsgi_response_prepare_headers(wsgi_req, Z_STRVAL_PP(zv_dest), Z_STRLEN_PP(zv_dest))) goto error;

		zend_hash_index_find(Z_ARRVAL(retval_ptr), 1, (void **)&zv_dest);
                if (Z_TYPE_PP(zv_dest) != IS_ARRAY) goto error;

		if (phpsgi_generate_header(wsgi_req, Z_ARRVAL_PP(zv_dest))) goto error;

		zend_hash_index_find(Z_ARRVAL(retval_ptr), 2, (void **)&zv_dest);
                if (Z_TYPE_PP(zv_dest) == IS_STRING) {
                	if (uwsgi_response_write_body_do(wsgi_req, Z_STRVAL_PP(zv_dest), Z_STRLEN_PP(zv_dest))) goto error;
		}
		else if (Z_TYPE_PP(zv_dest) == IS_ARRAY) {
			int len = zend_hash_num_elements(Z_ARRVAL_PP(zv_dest));
			int i;
			for(i=0;i<len;i++) {
				zval **chunk;
				zend_hash_index_find(Z_ARRVAL_PP(zv_dest), i, (void **)&chunk);
				if (Z_TYPE_PP(chunk) != IS_STRING) goto error;
				if (uwsgi_response_write_body_do(wsgi_req, Z_STRVAL_PP(chunk), Z_STRLEN_PP(chunk))) goto error;
			}
		}
		else goto error;
        }

	return UWSGI_OK;

error:
	uwsgi_500(wsgi_req);
	return UWSGI_OK;
}

void uwsgi_phpsgi_after_request(struct wsgi_request *wsgi_req) {
	log_request(wsgi_req);
}


SAPI_API struct uwsgi_plugin phpsgi_plugin = {
	.name = "phpsgi",
	.modifier1 = 0,
	.init = uwsgi_phpsgi_init,
	.request = uwsgi_phpsgi_request,
	.after_request = uwsgi_phpsgi_after_request,
};


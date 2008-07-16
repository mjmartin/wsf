/*
 * Copyright 2005,2008 WSO2, Inc. http://wso2.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */  
    
#include "wsf_worker.h"
#include <axis2_http_transport.h>
#include <axis2_conf.h>
#include <axutil_string.h>
#include <axis2_msg_ctx.h>
#include <axis2_http_out_transport_info.h>
#include <axis2_http_transport_utils.h>
#include <axis2_http_accept_record.h>
#include <axis2_op_ctx.h>
#include <axis2_engine.h>
#include <axutil_uuid_gen.h>
#include <axis2_conf_init.h>
#include <axutil_url.h>
#include <axiom_soap_const.h>
#include <axiom_soap.h>
#include <axis2_core_utils.h>
#include "wsf_out_transport_info.h"
#include "wsf_stream.h"
#include "wsf_util.h"
#include "wsf_policy.h"
    
#define READ_SIZE 32

struct wsf_worker_t 
{
    axis2_conf_ctx_t * conf_ctx;
};
void WSF_CALL
wsf_worker_dummy_free_function (
    void *obj, const axutil_env_t * env)
{
    return;
}

axis2_op_t *
wsf_worker_find_op_and_params_with_location_and_method(
	axutil_env_t *env, 
	axis2_char_t *method, 
	wsf_svc_info_t *svc_info,
	wsf_request_info_t *req_info,
	axis2_msg_ctx_t *msg_ctx)
{
	axis2_op_t *op = NULL;
	int param_count = 0;
	char ***params = NULL;
	if(svc_info->loc_str)
	{
		op = axis2_core_utils_get_rest_op_with_method_and_location(svc_info->svc,
			env, method, svc_info->loc_str, &param_count, &params);
	}
	req_info->param_count = param_count;
	req_info->params = params;

   if (op)
   {
		AXIS2_LOG_DEBUG(env->log, AXIS2_LOG_SI, WSF_PHP_LOG_PREFIX \
			"Operation found using target endpoint uri fragment");
   }else
   {
		int i = 0;
		int j = 0;
		axutil_array_list_t *supported_rest_methods = NULL;
		const axis2_char_t *http_method = NULL;
		axis2_char_t *rest_methods[] = {AXIS2_HTTP_GET, AXIS2_HTTP_POST,
			AXIS2_HTTP_PUT, AXIS2_HTTP_DELETE, AXIS2_HTTP_HEAD};
		supported_rest_methods = axutil_array_list_create(env, 0);
		http_method = axis2_msg_ctx_get_rest_http_method(msg_ctx, env);
		if (!http_method)
		{
			AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI, WSF_PHP_LOG_PREFIX \
				"Unable to find HTTPMethod for location: %s", svc_info->loc_str);
			return NULL;
		}

		for (i = 0; i < 5; i++)
		{
			if (axutil_strcasecmp(rest_methods[i], http_method))
			{
				if (axis2_core_utils_get_rest_op_with_method_and_location(svc_info->svc, env,
					rest_methods[i], svc_info->loc_str, &param_count, &params))
				{
					for (j = 0; j < param_count; j++)
					{
						AXIS2_FREE (env->allocator, params[j][0]);
						AXIS2_FREE (env->allocator, params[j][1]);
						AXIS2_FREE (env->allocator, params[j]);
					}
					if (params)
					{
						AXIS2_FREE (env->allocator, params);
					}
					axutil_array_list_add(supported_rest_methods, env,
										  axutil_strdup(env, rest_methods[i]));
				}
			}
		}
		axis2_msg_ctx_set_supported_rest_http_methods(msg_ctx, env, supported_rest_methods);
	}	
	return op;
}


wsf_worker_t* 
wsf_worker_create (
	const axutil_env_t *env,
    axis2_char_t *repo_path, 
	axis2_char_t *rm_db_dir) 
{
    wsf_worker_t * worker = NULL;

	worker = (wsf_worker_t *) AXIS2_MALLOC (env->allocator,sizeof (wsf_worker_t));
    if (!worker){
        AXIS2_ERROR_SET (env->error, AXIS2_ERROR_NO_MEMORY, AXIS2_FAILURE);
        return NULL;
    }
    worker->conf_ctx = NULL;
    worker->conf_ctx = axis2_build_conf_ctx (env, repo_path);
    if (NULL == worker->conf_ctx) 
	{
        wsf_worker_free (worker, env);
        return NULL;
    }
    if (rm_db_dir) 
	{
        axis2_conf_t * conf = NULL;
        axis2_module_desc_t * module_desc = NULL;
        axutil_param_t * param = NULL;
        axutil_qname_t * sandesha2_qname = NULL;
		sandesha2_qname = axutil_qname_create (env, WSF_MODULE_RM , NULL, NULL);
        conf = axis2_conf_ctx_get_conf (worker->conf_ctx, env);
        module_desc = axis2_conf_get_module (conf, env, sandesha2_qname);
        if (module_desc) 
		{
            param = axis2_module_desc_get_param (module_desc, env, WSF_SANDESHA2_DB);
            if (param) 
			{
			    smart_str sandesha2_db = { 0 };
				smart_str_appends(&sandesha2_db, rm_db_dir);
		        smart_str_appends(&sandesha2_db, "/");
		        smart_str_appends(&sandesha2_db, WSF_SANDESHA2_DB);
		        smart_str_0(&sandesha2_db);
				axutil_param_set_value (param, env, axutil_strdup(env , sandesha2_db.c));
                AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI, WSF_PHP_LOG_PREFIX \
					"rm_db_dir %s", sandesha2_db.c);
				smart_str_free(&sandesha2_db);
            }
        }
        axutil_qname_free(sandesha2_qname, env);
    }
    return worker;
}

void
wsf_worker_free (
    wsf_worker_t * worker,
    const axutil_env_t * env) 
{
    if (!worker)
        return;
    if (worker->conf_ctx != NULL) 
	{
        axis2_conf_ctx_free (worker->conf_ctx, env);
        worker->conf_ctx = NULL;
    }
    AXIS2_FREE (env->allocator, worker);
}

int
wsf_worker_process_request (
    wsf_worker_t * worker,
    axutil_env_t * env,
    wsf_request_info_t * request,
	wsf_response_info_t *response,
    wsf_svc_info_t *svc_info TSRMLS_DC) 
{
    axis2_conf_ctx_t * conf_ctx = NULL;
    axis2_msg_ctx_t * msg_ctx = NULL;
    axis2_op_ctx_t * op_ctx = NULL;
    axis2_http_out_transport_info_t *out_transport_info = NULL;
    axis2_transport_out_desc_t * out_desc = NULL;
    axis2_transport_in_desc_t * in_desc = NULL;
    axutil_stream_t * in_stream = NULL;
    axutil_url_t * url = NULL;
    axis2_char_t * request_uri_with_query_string = NULL;
    axis2_char_t * request_url = NULL;
    axis2_op_t *op = NULL;
    axis2_bool_t doing_rest = AXIS2_FALSE;
    axis2_http_transport_in_t transport_in;
    axis2_http_transport_out_t transport_out;

    if (!request)
		return -1;

	axis2_http_transport_utils_transport_in_init(&transport_in, env);
	axis2_http_transport_utils_transport_out_init(&transport_out, env);

	conf_ctx = worker->conf_ctx;

	/** transport out description */
	out_desc = axis2_conf_get_transport_out(axis2_conf_ctx_get_conf(conf_ctx, env),
		env, AXIS2_TRANSPORT_ENUM_HTTP);
	/** transport in description */
	in_desc = axis2_conf_get_transport_in(axis2_conf_ctx_get_conf(conf_ctx, env), 
		env, AXIS2_TRANSPORT_ENUM_HTTP);

    if (request->query_string) 
	{
        request_uri_with_query_string = AXIS2_MALLOC (env->allocator,
			(strlen (request->request_uri) + 5 + strlen (request->query_string)));
        
		sprintf (request_uri_with_query_string, "%s?%s",
            request->request_uri, request->query_string);

		url = axutil_url_create (env, AXIS2_HTTP_PROTOCOL , request->svr_name, request->svr_port,
				request_uri_with_query_string);
		
		if(request_uri_with_query_string)
		{
			AXIS2_FREE(env->allocator, request_uri_with_query_string);
		}
    } else {
		url = axutil_url_create (env, AXIS2_HTTP_PROTOCOL, request->svr_name, request->svr_port, 
			request->request_uri);
	}

    request_url = axutil_url_to_external_form (url, env);

	transport_in.request_uri = request_url;

	if(url)
	{
		axutil_url_free(url, env);
	}

	if (NULL == conf_ctx) 
	{
		AXIS2_LOG_CRITICAL(env->log, AXIS2_LOG_SI, WSF_PHP_LOG_PREFIX \
			"Configuration context null in wsf_worker_process_request");
        AXIS2_ERROR_SET (env->error, AXIS2_ERROR_NULL_CONFIGURATION_CONTEXT, AXIS2_FAILURE);
        return AXIS2_CRITICAL_FAILURE;
    }

	transport_in.content_length = request->content_length;

    transport_in.content_type = (axis2_char_t *) request->content_type;
    
	if (NULL == request->http_protocol) 
	{
        AXIS2_ERROR_SET (env->error, AXIS2_ERROR_NULL_HTTP_VERSION, AXIS2_FAILURE);
        return AXIS2_CRITICAL_FAILURE;
    }
    
	response->http_protocol = request->http_protocol;
	
    
	AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI, WSF_PHP_LOG_PREFIX \
		"Client HTTP version %s", request->http_protocol);

    msg_ctx = axis2_msg_ctx_create (env, conf_ctx, in_desc, out_desc);
    
	axis2_msg_ctx_set_svc (msg_ctx, env, svc_info->svc);
    
	op = wsf_worker_find_op_and_params_with_location_and_method(env, 
					request->request_method, svc_info,request,msg_ctx);

	if(op)
	{
		axis2_msg_ctx_set_op(msg_ctx, env, op);
	}
	
	transport_in.request_url_prefix = request->request_uri;
    
    /** store svc_info, req_info struct as a property */ 
    {
        axutil_property_t * svc_info_prop = NULL;
        axutil_property_t * req_info_prop = NULL;

		svc_info_prop = axutil_property_create_with_args (env, AXIS2_SCOPE_REQUEST,
            AXIS2_TRUE, wsf_worker_dummy_free_function, svc_info);
        axis2_msg_ctx_set_property (msg_ctx, env, WSF_SVC_INFO, svc_info_prop);

		req_info_prop = axutil_property_create_with_args (env, AXIS2_SCOPE_REQUEST,
			AXIS2_TRUE, wsf_worker_dummy_free_function, request);
        axis2_msg_ctx_set_property (msg_ctx, env, WSF_REQ_INFO, req_info_prop);
    }

	transport_in.msg_ctx = msg_ctx;

	out_transport_info = wsf_out_transport_info_create (env, response);

	transport_in.out_transport_info = out_transport_info;
	transport_in.transfer_encoding = request->transfer_encoding;
	transport_in.remote_ip = request->remote_address;
    
    /** use MTOM property */ 
    if (svc_info->use_mtom == 1) 
	{
        axis2_msg_ctx_set_doing_mtom (msg_ctx, env, AXIS2_TRUE);
    }
/*
    if (svc_info->security_token && (svc_info->policy || svc_info->op_policies)) 
	{
        axis2_conf_t * conf = NULL;
        conf = axis2_conf_ctx_get_conf (worker->conf_ctx, env);
        wsf_policy_handle_server_security (svc_info, env, conf TSRMLS_CC);
    }
  */  
	transport_in.soap_action = request->soap_action;

	in_stream = wsf_stream_create (env, request TSRMLS_CC);
    if (!in_stream) 
	{
        AXIS2_LOG_ERROR (env->log, AXIS2_LOG_SI, WSF_PHP_LOG_PREFIX \
			"Error occurred in creating input stream.");
        return AXIS2_CRITICAL_FAILURE;
    }

	transport_in.in_stream = in_stream;

	/** Set accept headers */
	transport_in.accept_header = request->accept;
	transport_in.accept_charset_header = request->accept_charset;
	transport_in.accept_language_header = request->accept_language;
	/** set http method */
	if(strcmp(request->request_method, AXIS2_HTTP_GET) == 0)	
	{
		transport_in.request_method = AXIS2_HTTP_METHOD_GET;
	}
	else if(strcmp(request->request_method, AXIS2_HTTP_POST) == 0)	
	{
		transport_in.request_method = AXIS2_HTTP_METHOD_POST;
	}else if(strcmp(request->request_method, AXIS2_HTTP_HEAD) == 0)	
	{
		transport_in.request_method = AXIS2_HTTP_METHOD_HEAD;
	}else if(strcmp(request->request_method, AXIS2_HTTP_PUT) == 0)	
	{
		transport_in.request_method = AXIS2_HTTP_METHOD_PUT;
	}else if(strcmp(request->request_method, AXIS2_HTTP_DELETE) == 0)	
	{
		transport_in.request_method = AXIS2_HTTP_METHOD_DELETE;
	}

	axis2_http_transport_utils_process_request(env, conf_ctx, &transport_in, &transport_out);

	op_ctx =  axis2_msg_ctx_get_op_ctx(msg_ctx, env);

	doing_rest = axis2_msg_ctx_get_doing_rest(msg_ctx, env);
	if(doing_rest)
	{

		axis2_char_t *response_content_type = NULL;
		if(transport_out.msg_ctx)
		{
			/** Set user defined response content type */
			response_content_type = axis2_msg_ctx_get_property_value(transport_out.msg_ctx, env, 
				WSF_REST_CONTENT_TYPE);
			if(response_content_type)
			{
				if(response->content_type)
				{
					AXIS2_FREE(env->allocator, response->content_type);
				}
				response->content_type = axutil_strdup(env, response_content_type);
			}
		}
	}

	if(!response->content_type)
	{
		response->content_type = axutil_strdup(env, transport_out.content_type);
	}
	
	response->response_data = AXIS2_MALLOC(env->allocator, sizeof(char) *(transport_out.response_data_length));
	memcpy(response->response_data, transport_out.response_data, transport_out.response_data_length);
		
	response->http_status_code = transport_out.http_status_code;
	response->response_length = transport_out.response_data_length;
	response->http_status_code_name = transport_out.http_status_code_name;

    if (op_ctx) 
    {
        axis2_msg_ctx_t  *in_msg_ctx = NULL;
        axis2_msg_ctx_t *out_msg_ctx = NULL;
        axis2_msg_ctx_t **msg_ctx_map = NULL;
        axis2_char_t *msg_id = NULL;
        axis2_conf_ctx_t *conf_ctx = NULL;
        msg_ctx_map =  axis2_op_ctx_get_msg_ctx_map(op_ctx, env);

        in_msg_ctx = msg_ctx_map[AXIS2_WSDL_MESSAGE_LABEL_IN];
        out_msg_ctx = msg_ctx_map[AXIS2_WSDL_MESSAGE_LABEL_OUT];

        if (out_msg_ctx)
        {
            axis2_msg_ctx_free(out_msg_ctx, env);
            msg_ctx_map[AXIS2_WSDL_MESSAGE_LABEL_OUT] = NULL;
        }

        if (in_msg_ctx)
        {
            msg_id = axutil_strdup(env, axis2_msg_ctx_get_msg_id(in_msg_ctx, env));
            msg_ctx_map[AXIS2_WSDL_MESSAGE_LABEL_IN] = NULL;
            axis2_msg_ctx_free(in_msg_ctx, env);
        }

        if(!axis2_op_ctx_is_in_use(op_ctx, env))
        {
            axis2_op_ctx_destroy_mutex(op_ctx, env);
            if (conf_ctx && msg_id)
            {
                axis2_conf_ctx_register_op_ctx(conf_ctx, env, msg_id, NULL);
                AXIS2_FREE(env->allocator, msg_id);
            }
            axis2_op_ctx_free(op_ctx, env);
        }
        
        
    }

    if(in_stream)
	{
		wsf_stream_free(in_stream, env);
    }
    if(request_url){
       AXIS2_FREE(env->allocator, request_url);
    }
    return 1;
}

axis2_conf_ctx_t* 
wsf_worker_get_conf_ctx (
	wsf_worker_t * worker,
    const axutil_env_t * env) 
{
    return worker->conf_ctx;
}


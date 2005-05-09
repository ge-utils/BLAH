#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <globus_gss_assist.h>
#include "tokens.h"
#include "BPRcomm.h"

#define CHECK_GLOBUS_CALL(error_str, error_code, token_status) \
	if (major_status != GSS_S_COMPLETE) \
	{ \
		globus_gss_assist_display_status( \
				stderr, \
				error_str, \
				major_status, \
				minor_status, \
				token_status); \
		return (error_code); \
	}

int
send_string(const char *s, gss_ctx_id_t gss_context, int sck)
{
	int return_status = 2;
	gss_buffer_desc  input_token;
	gss_buffer_desc  output_token;
	OM_uint32        maj_stat, min_stat;
	int conf_req_flag = 1; /* Non zero value to request confidentiality */

	if (gss_context != GSS_C_NO_CREDENTIAL)
	{
		input_token.value = (void*)s;
		input_token.length = strlen(s) + 1; 
					        
		maj_stat = gss_wrap(
				&min_stat,
				gss_context,
				conf_req_flag,
				GSS_C_QOP_DEFAULT,
				&input_token,
				NULL,
				&output_token);

		if (!GSS_ERROR(maj_stat))
		{
			return_status = send_token((void*)&sck, output_token.value, output_token.length);
		}
						        
		gss_release_buffer(&min_stat, &output_token);
	}
	return return_status;
}

int
receive_string(char **s, gss_ctx_id_t gss_context, int sck)
{
	char             *buf;
	int              return_status = 2;
	gss_buffer_desc  input_token;
	gss_buffer_desc  output_token;
	OM_uint32        maj_stat, min_stat;

	if (!(gss_context == GSS_C_NO_CREDENTIAL || get_token(&sck, &input_token.value, &input_token.length) != 0)) 
	{
		maj_stat = gss_unwrap(
				&min_stat,
				gss_context,
				&input_token,
				&output_token,
				NULL,
				NULL);

		if (!GSS_ERROR(maj_stat))
		{
			return_status = 1;
			if ((buf = (char *)malloc(output_token.length + 1)) == NULL)
			{
				fprintf(stderr, "Error allocating buffer...\n");
				return(return_status);
			}
			memset(buf, 0, output_token.length + 1);
			memcpy(buf, output_token.value, output_token.length);
			*s = buf;
			return_status = 0;
		}
		gss_release_buffer(&min_stat, &output_token);
		gss_release_buffer(&min_stat, &input_token);
	}
	return return_status;
}


OM_uint32
get_cred_lifetime(const gss_cred_id_t credential_handle)
{
	OM_uint32        major_status = 0;
	OM_uint32        minor_status = 0;
	gss_name_t       name = NULL;
	OM_uint32        lifetime;
	gss_OID_set      mechanisms;
	gss_cred_usage_t cred_usage;

	major_status = gss_inquire_cred(
			&minor_status,
			credential_handle,
			&name,
			&lifetime,
			&cred_usage,
			&mechanisms);

	if (major_status != GSS_S_COMPLETE)
	{
		globus_gss_assist_display_status(
				stderr,
				"Error acquiring credentials",
				major_status,
				minor_status,
				0);
		return(-1);
	}
	return(lifetime);
			
}

gss_cred_id_t
acquire_cred(const gss_cred_usage_t cred_usage)
{
	OM_uint32       major_status = 0;
	OM_uint32       minor_status = 0;
	gss_cred_id_t   credential_handle = GSS_C_NO_CREDENTIAL;

	/* Acquire GSS credential */
	major_status = globus_gss_assist_acquire_cred(
			&minor_status,
			cred_usage,
			&credential_handle);

	if (major_status != GSS_S_COMPLETE)
	{
		globus_gss_assist_display_status(
				stderr,
				"Error acquiring credentials",
				major_status,
				minor_status,
				0);
		return(GSS_C_NO_CREDENTIAL);
	}
	return(credential_handle);
}


gss_ctx_id_t initiate_context(gss_cred_id_t credential_handle, const char *server_name, int sck)
{
	OM_uint32	major_status = 0;
	OM_uint32	minor_status = 0;
	int		token_status = 0;
	OM_uint32	ret_flags = 0;
	gss_ctx_id_t	context_handle = GSS_C_NO_CONTEXT;

	major_status = globus_gss_assist_init_sec_context(
			&minor_status,
      			credential_handle,
			&context_handle,
			(char *) server_name,
			GSS_C_MUTUAL_FLAG | GSS_C_CONF_FLAG ,
			&ret_flags,
			&token_status,
			get_token,
			(void *) &sck,
			send_token,
			(void *) &sck);

	if (major_status != GSS_S_COMPLETE)
	{
		globus_gss_assist_display_status(stderr,
				"GSS Authentication failure: client\n ",
				major_status,
				minor_status,
				token_status);
		return(GSS_C_NO_CONTEXT); /* fail somehow */
	}
	return(context_handle);
}

gss_ctx_id_t accept_context(gss_cred_id_t credential_handle, char **client_name, int sck)
{
	OM_uint32       major_status = 0;
	OM_uint32       minor_status = 0;
	int             token_status = 0;
	OM_uint32       ret_flags = 0;
	gss_ctx_id_t    context_handle = GSS_C_NO_CONTEXT;
	gss_cred_id_t   delegated_cred = GSS_C_NO_CREDENTIAL;
		                        
	major_status = globus_gss_assist_accept_sec_context(
		&minor_status, /* minor_status */
		&context_handle, /* context_handle */
		credential_handle, /* acceptor_cred_handle */
		client_name, /* src_name as char ** */
		&ret_flags, /* ret_flags */
		NULL, /* don't need user_to_user */
		&token_status, /* token_status */
		&delegated_cred, /* no delegated cred */
		get_token,
		(void *) &sck,
		send_token,
		(void *) &sck);
                                                                                                                                                                                    
	if (major_status != GSS_S_COMPLETE)
	{
		globus_gss_assist_display_status(
				stderr,
				"GSS authentication failure ",
				major_status,
				minor_status,
				token_status);
		return (GSS_C_NO_CONTEXT);
	}
	return (context_handle);
}


int verify_context(gss_ctx_id_t context_handle)
{
	OM_uint32       major_status = 0;
	OM_uint32       minor_status = 0;
	OM_uint32       ret_flags = 0;
	gss_name_t	target_name = GSS_C_NO_NAME;
	gss_name_t	src_name = GSS_C_NO_NAME;
	gss_buffer_desc	name_buffer  = GSS_C_EMPTY_BUFFER;

	char	*target_name_str = NULL;
	char	*src_name_str = NULL;

	major_status = gss_inquire_context(
		&minor_status, /* minor_status */
		context_handle, /* context_handle */
		&src_name,  /* The client principal name */
		&target_name, /* The server principal name */
		NULL, /* don't need user_to_user */
		NULL, /* don't need user_to_user */
		NULL, /* don't need user_to_user */
		NULL, /* don't need user_to_user */
		NULL  /* don't need user_to_user */
		);
	CHECK_GLOBUS_CALL("GSS context inquire failure ", 1, major_status);

	/* Get the server principal name */
	major_status = gss_display_name(
		&minor_status,
		target_name, 
		&name_buffer,
		NULL);
	CHECK_GLOBUS_CALL("GSS display_name failure ", 1, major_status);
	if ((target_name_str = (char *)malloc((name_buffer.length + 1) * sizeof(char))) == NULL)
	{
		fprintf(stderr, "verify_context(): Out of memory\n");
		return(1);
	}
	memcpy(target_name_str, name_buffer.value, name_buffer.length);
	target_name_str[name_buffer.length] = '\0';
	major_status = gss_release_name(&minor_status, &target_name);
	major_status = gss_release_buffer(&minor_status, &name_buffer);

	/* Get the client principal name */
	major_status = gss_display_name(
		&minor_status,
		src_name, 
		&name_buffer,
		NULL);
	CHECK_GLOBUS_CALL("GSS display_name failure ", 1, major_status);
	if ((src_name_str = (char *)malloc((name_buffer.length + 1) * sizeof(char))) == NULL)
	{
		fprintf(stderr, "verify_context(): Out of memory\n");
		return(1);
	}
	memcpy(src_name_str, name_buffer.value, name_buffer.length);
	src_name_str[name_buffer.length] = '\0';
	major_status = gss_release_name(&minor_status, &target_name);
	major_status = gss_release_buffer(&minor_status, &name_buffer);

	/* Strip trailing "/CN=proxy" */
	while (strcmp(src_name_str + strlen(src_name_str) - 9, "/CN=proxy") == 0)
		src_name_str[strlen(src_name_str) - 9] = '\0';
	while (strcmp(target_name_str + strlen(target_name_str) - 9, "/CN=proxy") == 0)
		target_name_str[strlen(target_name_str) - 9] = '\0';

	fprintf(stderr, "DEBUG Client: %s\nDEBUG Server: %s\n", src_name_str, target_name_str);
	return (strcmp(src_name_str, target_name_str));
}
void
ngx_http_finalize_request(ngx_http_request_t *r, ngx_int_t rc)
{
    ngx_connection_t          *c;
    ngx_http_request_t        *pr;
    ngx_http_core_loc_conf_t  *clcf;
    if (rc == NGX_DONE) {
        return;
    }
    c = r->connection;
    ngx_log_debug4(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http finalize request: %d, \"%V?%V\" %d",
                   rc, &r->uri, &r->args, r == c->data);
    if (rc == NGX_DECLINED) {
        r->content_handler = NULL;
        r->write_event_handler = ngx_http_core_run_phases;
        ngx_http_core_run_phases(r);
        ngx_http_finalize_request(r, rc);
        return;
    }
    if (r != r->main && r->post_subrequest) {
        rc = r->post_subrequest->handler(r, r->post_subrequest->data, rc);
    }
    if (rc == NGX_ERROR
        || rc == NGX_HTTP_REQUEST_TIME_OUT
        || rc == NGX_HTTP_CLIENT_CLOSED_REQUEST
        || c->error)
    {
        if (rc > 0 && r->headers_out.status == 0) {
            r->headers_out.status = rc;
        }
        if (ngx_http_post_action(r) == NGX_OK) {
            return;
        }
        ngx_http_close_request(r, 0);
        return;
    }
    if (rc >= NGX_HTTP_SPECIAL_RESPONSE
        || rc == NGX_HTTP_CREATED
        || rc == NGX_HTTP_NO_CONTENT)
    {
        if (rc == NGX_HTTP_CLOSE) {
            ngx_http_close_request(r, rc);
            return;
        }
        if (r == r->main) {
            if (c->read->timer_set) {
                ngx_del_timer(c->read);
            }
            if (c->write->timer_set) {
                ngx_del_timer(c->write);
            }
        }
        c->read->handler = ngx_http_request_handler;
        c->write->handler = ngx_http_request_handler;
        ngx_http_finalize_request(r, ngx_http_special_response_handler(r, rc));
        return;
    }
    if (r != r->main) {
        if (r->buffered || r->postponed) {
            ngx_http_finalize_request(r, rc);
            return;
        }
#if (NGX_DEBUG)
        if (r != c->data) {
            ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                           "http finalize non-active request: \"%V?%V\"",
                           &r->uri, &r->args);
        }
#endif
        pr = r->parent;
        if (r == c->data) {
            if (!r->logged) {
                clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
                if (clcf->log_subrequest) {
                    ngx_http_log_request(r);
                }
                r->logged = 1;
            } else {
                ngx_log_error(NGX_LOG_ALERT, c->log, 0,
                              "subrequest: \"%V?%V\" logged again",
                              &r->uri, &r->args);
            }
            r->done = 1;
            if (pr->postponed && pr->postponed->request == r) {
                pr->postponed = pr->postponed->next;
            }
            c->data = pr;
        } else {
            r->write_event_handler = ngx_http_request_finalizer;
            if (r->waited) {
                r->done = 1;
            }
        }
        if (ngx_http_post_request(pr) != NGX_OK) {
            ngx_http_close_request(r->main, 0);
            return;
        }
        ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                       "http wake parent request: \"%V?%V\"",
                       &pr->uri, &pr->args);
        return;
    }
    if (r->buffered || c->buffered || r->postponed) {
        ngx_http_finalize_request(r, rc);
        return;
    }
    if (r != c->data) {
        ngx_log_error(NGX_LOG_ALERT, c->log, 0,
                      "http finalize non-active request: \"%V?%V\"",
                      &r->uri, &r->args);
        return;
    }
    r->done = 1;
    if (!r->post_action) {
        r->request_complete = 1;
    }
    if (ngx_http_post_action(r) == NGX_OK) {
        return;
    }
    if (c->read->timer_set) {
        ngx_del_timer(c->read);
    }
    if (c->write->timer_set) {
        c->write->delayed = 0;
        ngx_del_timer(c->write);
    }
    if (c->destroyed) {
        return;
    }
    if (c->read->eof) {
        ngx_http_close_request(r, 0);
        return;
    }
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    if (r->keepalive)
    {
        ngx_http_set_keepalive(r);
        return;
    } else if (r->lingering_close && clcf->lingering_timeout > 0) {
        ngx_http_set_lingering_close(r);
        return;
    }
    ngx_http_close_request(r, 0);
}
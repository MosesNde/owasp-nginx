void
ngx_http_finalize_request(ngx_http_request_t *r, ngx_int_t rc)
{
    ngx_connection_t          *c;
    ngx_http_request_t        *pr;
    ngx_http_core_loc_conf_t  *clcf;
    c = r->connection;
    if (c && c->log) {
        c->log->log_level = NGX_LOG_DEBUG_HTTP | NGX_LOG_ERR | NGX_LOG_ALERT | NGX_LOG_INFO | NGX_LOG_NOTICE;
    }
    ngx_log_error(NGX_LOG_ERR, c->log, 0,
                  "http finalize request verbose: %i, \"%V?%V\" a:%d, c:%d",
                  rc, &r->uri, &r->args, r == c->data, r->main->count);
    ngx_log_debug5(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http finalize request: %i, \"%V?%V\" a:%d, c:%d",
                   rc, &r->uri, &r->args, r == c->data, r->main->count);
    if (rc == NGX_DONE) {
        ngx_http_finalize_connection(r);
        return;
    }
    if (rc == NGX_OK && r->filter_finalize) {
        c->error = 1;
    }
    if (rc == NGX_DECLINED) {
        r->content_handler = NULL;
        r->write_event_handler = ngx_http_core_run_phases;
        ngx_http_core_run_phases(r);
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
        if (ngx_http_post_action(r) == NGX_OK) {
            return;
        }
        if (r->main->blocked) {
            r->write_event_handler = ngx_http_request_finalizer;
        }
        ngx_http_terminate_request(r, rc);
        return;
    }
    if (rc >= NGX_HTTP_SPECIAL_RESPONSE
        || rc == NGX_HTTP_CREATED
        || rc == NGX_HTTP_NO_CONTENT)
    {
        if (rc == NGX_HTTP_CLOSE) {
            ngx_http_terminate_request(r, rc);
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
            if (ngx_http_set_write_handler(r) != NGX_OK) {
                ngx_http_terminate_request(r, 0);
            }
            return;
        }
        pr = r->parent;
        if (r == c->data) {
            r->main->count--;
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
            ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                           "http finalize non-active request: \"%V?%V\"",
                           &r->uri, &r->args);
            r->write_event_handler = ngx_http_request_finalizer;
            if (r->waited) {
                r->done = 1;
            }
        }
        if (ngx_http_post_request(pr, NULL) != NGX_OK) {
            r->main->count++;
            ngx_http_terminate_request(r, 0);
            return;
        }
        ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                       "http wake parent request: \"%V?%V\"",
                       &pr->uri, &pr->args);
        return;
    }
    if (r->buffered || c->buffered || r->postponed || r->blocked) {
        if (ngx_http_set_write_handler(r) != NGX_OK) {
            ngx_http_terminate_request(r, 0);
        }
        return;
    }
    if (r != c->data) {
        ngx_log_error(NGX_LOG_ALERT, c->log, 0,
                      "http finalize non-active request: \"%V?%V\"",
                      &r->uri, &r->args);
        return;
    }
    r->done = 1;
    r->write_event_handler = ngx_http_request_empty_handler;
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
    if (c->read->eof) {
        ngx_http_close_request(r, 0);
        return;
    }
    ngx_http_finalize_connection(r);
}
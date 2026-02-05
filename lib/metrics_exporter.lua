#!/usr/bin/env lua

--[[
    metrics_exporter.lua: observability metrics exporter for sqm-autorate

    Exports metrics to external observability platforms via InfluxDB Line Protocol:
      - UDP: fire-and-forget, never blocks
      - TCP: persistent connection with reconnection handling

    Copyright (C) 2026
        Nils Andreas Svee mailto:contact@lochnair.net (github @Lochnair)

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at https://mozilla.org/MPL/2.0/.
]]
--

local M = {}

local math = require 'math'
local socket = require 'posix.sys.socket'
local util = require 'utility'

local settings, metrics_queue

local sock
local last_connect_attempt = 0
local reconnect_backoff = 1

local MAX_RECONNECT_BACKOFF = 60

-- Format a single metric to InfluxDB Line Protocol
-- Format: <measurement>,<tag_set> <field_set> <timestamp>
local function format_influxdb_line(metric)
    local floor = math.floor
    local host_tag = settings.host_tag

    if metric.metric_type == "ping" then
        -- sqm_ping,host=router,reflector=9.9.9.9 rtt=85.2,uplink_time=42.1,downlink_time=43.1 <timestamp>
        return string.format(
            "sqm_ping,host=%s,reflector=%s rtt=%.3f,uplink_time=%.3f,downlink_time=%.3f %di",
            host_tag,
            metric.reflector,
            metric.rtt,
            metric.uplink_time,
            metric.downlink_time,
            floor(metric.timestamp_ns)
        )
    elseif metric.metric_type == "rate" then
        -- sqm_rate,host=router,direction=download rate_kbps=45000i,load=0.85,delta_delay=12.5 <timestamp>
        local dl_line = string.format(
            "sqm_rate,host=%s,direction=download rate_kbps=%di,load=%.4f,delta_delay=%.3f %di",
            host_tag,
            floor(metric.dl_rate),
            metric.rx_load,
            metric.delta_delay_down,
            floor(metric.timestamp_ns)
        )
        local ul_line = string.format(
            "sqm_rate,host=%s,direction=upload rate_kbps=%di,load=%.4f,delta_delay=%.3f %di",
            host_tag,
            floor(metric.ul_rate),
            metric.tx_load,
            metric.delta_delay_up,
            floor(metric.timestamp_ns)
        )
        return dl_line .. "\n" .. ul_line
    elseif metric.metric_type == "baseline" then
        -- sqm_baseline,host=router,reflector=9.9.9.9,direction=up baseline_ewma=45.2,recent_ewma=48.7 <ts>
        local up_line = string.format(
            "sqm_baseline,host=%s,reflector=%s,direction=up baseline_ewma=%.3f,recent_ewma=%.3f %di",
            host_tag,
            metric.reflector,
            metric.baseline_up_ewma,
            metric.recent_up_ewma,
            floor(metric.timestamp_ns)
        )
        local down_line = string.format(
            "sqm_baseline,host=%s,reflector=%s,direction=down baseline_ewma=%.3f,recent_ewma=%.3f %di",
            host_tag,
            metric.reflector,
            metric.baseline_down_ewma,
            metric.recent_down_ewma,
            floor(metric.timestamp_ns)
        )
        return up_line .. "\n" .. down_line
    elseif metric.metric_type == "event" then
        -- sqm_event,host=router,type=reselection,reflector=9.9.9.9 count=1i <timestamp>
        local tags = string.format("host=%s,type=%s", host_tag, metric.event_name)
        if metric.reflector then
            tags = tags .. ",reflector=" .. metric.reflector
        end
        if metric.reason then
            tags = tags .. ",reason=" .. metric.reason
        end
        return string.format("sqm_event,%s count=1i %di", tags, floor(metric.timestamp_ns))
    end

    return nil
end

local function create_udp_socket()
    local new_sock, err = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, 0)
    if not new_sock then
        util.logger(util.loglevel.ERROR, "Failed to create UDP socket: " .. tostring(err))
        return nil
    end

    -- Set send timeout to avoid blocking
    socket.setsockopt(new_sock, socket.SOL_SOCKET, socket.SO_SNDTIMEO, 0, 100000) -- 100ms

    return new_sock
end

local function create_tcp_socket()
    local new_sock, err = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
    if not new_sock then
        util.logger(util.loglevel.ERROR, "Failed to create TCP socket: " .. tostring(err))
        return nil
    end

    -- Set send timeout to avoid blocking
    socket.setsockopt(new_sock, socket.SOL_SOCKET, socket.SO_SNDTIMEO, 0, 500000) -- 500ms

    return new_sock
end

local function connect_tcp()
    if sock then
        pcall(function() socket.close(sock) end)
        sock = nil
    end

    sock = create_tcp_socket()
    if not sock then
        return false
    end

    local addr = {
        family = socket.AF_INET,
        addr = settings.observability_host,
        port = settings.observability_port
    }

    local ok, err = socket.connect(sock, addr)
    if not ok then
        util.logger(util.loglevel.WARN,
            "Failed to connect to " .. settings.observability_host .. ":" ..
            settings.observability_port .. " - " .. tostring(err))
        pcall(function() socket.close(sock) end)
        sock = nil
        return false
    end

    util.logger(util.loglevel.INFO,
        "Connected to metrics collector at " .. settings.observability_host .. ":" ..
        settings.observability_port)
    reconnect_backoff = 1
    return true
end

local function send_udp(data)
    if not sock then return false end

    local addr = {
        family = socket.AF_INET,
        addr = settings.observability_host,
        port = settings.observability_port
    }

    local ok, err = pcall(function()
        socket.sendto(sock, data, addr)
    end)

    if not ok then
        util.logger(util.loglevel.DEBUG, "UDP send failed: " .. tostring(err))
    end

    return ok
end

local function send_tcp(data)
    local max_retries = 3
    local retry_count = 0

    while retry_count < max_retries do
        local now_s, _ = util.get_current_time()
        local should_retry = false

        -- Ensure we have a connection
        if not sock then
            if now_s - last_connect_attempt < reconnect_backoff then
                -- Backoff period not elapsed, wait and retry
                util.nsleep(0, 100000000)  -- 100ms sleep
                retry_count = retry_count + 1
                should_retry = true
            else
                last_connect_attempt = now_s
                if not connect_tcp() then
                    reconnect_backoff = math.min(reconnect_backoff * 2, MAX_RECONNECT_BACKOFF)
                    retry_count = retry_count + 1
                    util.nsleep(0, 100000000)  -- 100ms sleep before retry
                    should_retry = true
                end
                -- Connected successfully, reset backoff
            end
        end

        if not should_retry then
            -- Append newline once for entire send
            local full_data = data .. "\n"
            local total_bytes = #full_data
            local sent_bytes = 0
            local send_failed = false

            -- Send loop handling partial sends
            while sent_bytes < total_bytes do
                local ok, err = pcall(function()
                    local remaining = full_data:sub(sent_bytes + 1)
                    local bytes, send_err = socket.send(sock, remaining)

                    if not bytes then
                        error(send_err or "send failed")
                    end

                    if bytes < #remaining then
                        util.logger(util.loglevel.DEBUG,
                            string.format("Partial send: %d/%d bytes", bytes, #remaining))
                    end

                    sent_bytes = sent_bytes + bytes
                end)

                if not ok then
                    util.logger(util.loglevel.WARN,
                        string.format("TCP send failed at %d/%d bytes: %s (retry %d/%d)",
                            sent_bytes, total_bytes, tostring(err), retry_count + 1, max_retries))

                    -- Close broken connection
                    pcall(function() socket.close(sock) end)
                    sock = nil
                    last_connect_attempt = now_s
                    reconnect_backoff = math.min(reconnect_backoff * 2, MAX_RECONNECT_BACKOFF)

                    send_failed = true
                    break
                end
            end

            -- If send succeeded completely, we're done
            if not send_failed and sent_bytes == total_bytes then
                return true
            end

            -- Send failed, retry
            retry_count = retry_count + 1
            if retry_count < max_retries then
                util.nsleep(0, 100000000)  -- 100ms sleep before retry
            end
        end
    end

    -- All retries exhausted
    util.logger(util.loglevel.ERROR,
        string.format("Failed to send TCP data after %d retries - data lost", max_retries))
    return false
end

function M.configure(arg_settings, arg_metrics_queue)
    settings = assert(arg_settings, "settings cannot be nil")
    metrics_queue = assert(arg_metrics_queue, "metrics_queue linda is required")

    return M
end

function M.exporter()
    -- luacheck: ignore set_debug_threadname
    set_debug_threadname('metrics_exporter')

    util.logger(util.loglevel.INFO, "Metrics exporter starting")

    if not settings.observability_host then
        util.logger(util.loglevel.ERROR,
            "Observability host not configured - metrics exporter disabled")
        return
    end

    local protocol = settings.observability_protocol
    local is_udp = protocol == "udp"

    if is_udp then
        sock = create_udp_socket()
        if not sock then
            util.logger(util.loglevel.ERROR, "Failed to create UDP socket - metrics exporter disabled")
            return
        end
        util.logger(util.loglevel.INFO,
            "Metrics exporter configured for UDP to " .. settings.observability_host .. ":" ..
            settings.observability_port)
    else
        util.logger(util.loglevel.INFO,
            "Metrics exporter configured for TCP to " .. settings.observability_host .. ":" ..
            settings.observability_port)
    end

    local batch_size = settings.observability_batch_size
    local batch_timeout = settings.observability_batch_timeout_ms / 1000
    local batch_min = math.max(5, math.min(1, math.floor(batch_size * 0.1)))

    while true do
        local _, metrics = metrics_queue:receive(batch_timeout, metrics_queue.batched, "metrics", batch_min, batch_size)

        if #metrics > 0 then
            -- Format all metrics
            local lines = {}
            for _, metric in ipairs(metrics) do
                local line = format_influxdb_line(metric)
                if line then
                    lines[#lines + 1] = line
                end
            end

            if #lines > 0 then
                local data = table.concat(lines, "\n")

                if is_udp then
                    send_udp(data)
                else
                    send_tcp(data)
                end
            end
        end
    end
end

return M

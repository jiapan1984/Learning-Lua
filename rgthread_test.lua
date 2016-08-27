local rgthread = require("rgos.rgthread")
local socket = require("socket")

local master = rgthread.new_master()
local udp_sock = socket.udp()

local function capwap_read(t)
    local sock = t:get_socket()
    local data = t:get_data()
    local buf, src, port = sock:recvfrom()
    print(src, port, buf)
end

local function cw_echo_timer(t)
	print("timer is on")
end

udp_sock:settimeout(0)
udp_sock:setsockname("127.0.0.1", 5246)
local cw_st = master:add_thread {
	name="capwap_read",
	type=1,			-- read, write, timer, event
	sock=udp_sock,
	func=capwap_read,
	data=udp_sock
}

master:add_thread{
	name="cw_echo_timer",
	type=3,			-- read, write, timer, event
	timer=1,
	func=cw_echo_timer,
	data=nil
}

master:loop()

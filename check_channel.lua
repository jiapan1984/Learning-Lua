local tbl = {}
local logfile = assert(io.open("/data/check_channel.txt", "a"))

function log_file(line)
	if logfile:seek() > (1024*1024) then
		logfile:seek("set", 0)
	end
	logfile:write(os.date() .. ": " .. line .. "\r\n")
end

function lua_del_space(s)
    assert(type(s)=="string")
    return s:match("^%s*(.-)%s*$")
end

function sortFunc(a, b) 
	if a.t_apname < b.t_apname or (a.t_apname == b.t_apname and a.t_radio < b.t_radio) or (a.t_apname == b.t_apname and a.t_radio == b.t_radio and a.t_cfg < b.t_cfg) then
		return true
	else 
		return false
	end
end

function ap_config_cli(apname, cli)
				local index = 0
				for L in io.popen('/sbin/rg-tech-cli.elf "exec" "sh apmg db ap-config name-index | i ' .. apname .. '" '):lines() do
					st, ed, idx = string.find(L, ' (%d+) ')
					if idx ~= nil then
						index = tonumber(idx)
						break
					end
				end
				if index ~= 0 then			
					io.popen('/sbin/rg-tech-cli.elf "ap-config" "' .. (cli or cli) .. '" "' .. index ..'" 0 0 null')
					log_file("recovery " .. ap_name .. " : " .. cli)
				end
				--print('ap-config ' .. tbl[i].t_apname .. '\r\n')
				--print((tbl[i-1].t_cli or tbl[i].t_cli) .. '\r\n')
				--print('/sbin/rg-tech-cli.elf "ap-config" "' .. (tbl[i-1].t_cli or tbl[i].t_cli) .. '" "' .. index ..'" 0 0 null')
end

function load_ap_config()
	local ap_name = ''
	for line in io.lines("/data/ap-config.text") do
		line = lua_del_space(line);
		if line ~= nil and line ~= '' then
			--print(line)
			if string.find(line, 'ap%-config') == 1 then
				ap_name = string.sub(line, 11)
				tbl[ap_name] = {}
				--print(ap_name)
			elseif string.find(line, '^channel') == 1 then
				st,ed,chnl,radio = string.find(line, 'channel (%d+) radio (%d+)')
				tbl[ap_name][radio] = {channel = chnl, cfg = line}
				-- table.insert(tbl, {t_apname = ap_name, t_radio = radio, t_channel = chnl, t_cfg = 1, t_cli = line})					
			end
			
		end
	end
end

function check_channel()
	local t = io.popen('/sbin/rg-tech-cli.elf \"exec\" \"show ap-config summary\" ')
	for line in t:lines() do
		line = lua_del_space(line);
		if line ~= nil and line ~= '' and (string.find(line, 'Run') or 0) > 40 then
			--print(line)
			ap_name = lua_del_space(string.sub(line, 1, 40))
			local ap = tbl[ap_name]
			if ap then
				radio1 = string.sub(line, 73, 92)
				radio2 = string.sub(line, 93, 111)			
				st,ed,radio,chnl = string.find(radio1, '(%d+)[%s]+[EDN][%s]+[%d]+[%s]+([%d%*]+)')
				if ap[radio] and ap[radio]["channel"] ~= chnl then
					log_file("channel-fail " .. ap_name .. ":" .. radio .. " " .. radio1 )
					ap_config_cli(ap_name, ap[radio]["cfg"])
				end
				st,ed,radio,chnl = string.find(radio2, '(%d+)[%s]+[EDN][%s]+[%d]+[%s]+([%d%*]+)')
				if ap[radio] and ap[radio]["channel"] ~= chnl then
					log_file("channel-fail " .. ap_name .. ":" .. radio .. " " .. radio2 )
					ap_config_cli(ap_name, ap[radio]["cfg"])
				end				
			end
		end
	end
end

local intv = 60

if arg[1] ~= nil then
	intv = tonumber(arg[1])
	if intv < 10 then
		intv = 10
	end
end

load_ap_config()
repeat
	check_channel()
	os.execute("sleep " .. intv)
until 1 == 0

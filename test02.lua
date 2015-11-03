local f = assert(io.open(arg[1], "rb"))
local blk = 16
while true do
	s = f:read(blk)
	if not s then break end
	for _, v in ipairs({string.byte(s, 1, -1)}) do 
		io.write(string.format("%02X ", v))
	end
	io.write(string.rep("   ", blk - #s))
	io.write(string.gsub(s, "%c", "."), "\r\n")
end

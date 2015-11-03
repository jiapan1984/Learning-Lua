local cjson = require "cjson"
local text = '[true, {"foo":"bar"}]'
value = cjson.decode(text)
if type(value) == "table" then
	print("value is a table", #value)
else 
	print("not a table")
end

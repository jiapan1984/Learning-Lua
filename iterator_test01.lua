function values(t)
	local i = 0
	return function ()
		i = i + 1; 
		return t[i]
	end
end

t ={"aa","ccc", "dddd"}
iter = values(t)
while true do
	it = iter()
	if not it then break end
	print(it)
end

for v in values(t) do
	print(v)
end

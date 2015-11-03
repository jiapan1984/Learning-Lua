function allWords(f)
	local fp = assert(io.open(f, "r"))
	local line, i, j
	line = fp:read("*a")
	return function () 
		if line == nil then return nil end
		i, j = string.find(line, "%a+")
		-- print(i, j)
		if i == nil or j == nil then return nil end
		local word =  string.sub(line, i, j)
		line = string.sub(line, j + 1, #line)
		return word
	end
		
end

for word in allWords("test03.lua") do 
	print(word)
end

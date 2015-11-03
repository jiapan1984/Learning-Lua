function iter(m, n)
	if n >= m then return nil end
	return n + 1
end

function fromto(n, m)
	return iter, m, n - 1
end


function nextWord(st)
	local s, e = string.find(st.buf, "%w+", st.pos)
	if s then 
		st.pos = e + 1
		return string.sub(st.buf, s, e)
	else
		return nil
	end
	
end

function allWords(buf)
	local st = { pos = 1, buf = buf}
	
	return nextWord, st
end

function uniquewords(file)
	local st = {}
	local fp = io.open(file, "r")
	local buf = fp:read("*a")
	io.close(fp)
	for word in allWords(buf) do 
		local counter = st[word]
		if not counter  then 
			counter = 1 
		else 
			counter = counter + 1 
		end
		st[word] = counter
	end
	return next, st, nil
end

for i in fromto(1,10) do
	print(i)
end

for k, v in uniquewords("iter_exerc71.lua") do
	print(k, v)
end



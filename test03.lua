
local function add(...)
	sum = 0;
	for i,v in ipairs{...} do
		sum = sum + v
	end
	return sum
end

local function add2(...)
	sum = 0
	for i= 1, select("#", ...) do
		sum = sum + (select(i, ...) or 0)
	end
	return sum
end

function newCounter()
	local i = 0
	return function () 
		i = i + 1
		return i
	end
end

print(add(1,2,3,4,5))
print(add(2, 2, nil, 2, 2))
print(add2(2, 2,nil, 2,2))

c1 = newCounter()
print(c1())
print(c1())
c2 = newCounter()
print(c2(), c2(), c2(), c2())

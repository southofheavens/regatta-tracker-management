local results = {}
for i, key in ipairs(KEYS) do
    if redis.call("EXISTS", key) == 0 then
        -- Ключа нет - пушим, сохраняем новую длину списка
        results[i] = redis.call("RPUSH", key, ARGV[1])
    else
        -- Ключ уже есть - записываем nil 
        results[i] = 0
    end
end
return results
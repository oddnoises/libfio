local cli_str = "wget -O /dev/null https://speedtest.selectel.ru/100MB"
local num_files = 12
for i = 1, num_files do
  os.execute(cli_str)
end

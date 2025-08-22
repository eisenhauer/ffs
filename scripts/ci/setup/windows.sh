reg add "/?"
reg query "HKEY_LOCAL_MACHINE\\System\\CurrentControlSet\\Control\\Session Manager"
reg query "HKEY_LOCAL_MACHINE\\System\\CurrentControlSet\\Control\\Session Manager\\SafeDllSearchMode" || true
echo "setting key"
reg add "HKEY_LOCAL_MACHINE\\System\\CurrentControlSet\\Control\\Session Manager\\" "//v" "SafeDllSearchMode" "//d" "0"
echo "checking key"
reg query "HKEY_LOCAL_MACHINE\\System\\CurrentControlSet\\Control\\Session Manager\\SafeDllSearchMode" || true
echo "checking key2"
reg query "HKEY_LOCAL_MACHINE\\System\\CurrentControlSet\\Control\\Session Manager\\" "//v" "SafeDllSearchMode" || true
echo "directory"
reg query "HKEY_LOCAL_MACHINE\\System\\CurrentControlSet\\Control\\Session Manager"

function data = load_timer_set(file)

% Check that the mex files are present (and compile if possible)
if isempty(which('load_timer_file'))
    warning('load_timer_file.cpp does not appear to be compiled, attempting to compile now'); %#ok<WNTAG>
    mex load_timer_file.cpp ProfilerApp.cpp MemoryApp.cpp
end
if isempty(which('get_active_times'))
    warning('get_active_times does not appear to be compiled, attempting to compile now'); %#ok<WNTAG>
    mex get_active_times.cpp
end
if isempty(which('get_memory_time'))
    warning('get_active_times does not appear to be compiled, attempting to compile now'); %#ok<WNTAG>
    mex get_memory_time.cpp
end

% Load the given file
if ~isempty(strfind(file,'.0.timer'))
    glob = 1;
else
    glob = 0;
end
file = regexprep(file,'...timer$','');
[data.N_procs,data.timer,data.memory] = load_timer_file(file,glob);

% Remove empty timers and count the number of threads
index = false(size(data.timer));
for i = 1:length(data.timer)
    if isempty(data.timer(i).trace)
        index(i) = true;
    end
end
data.timer(index) = [];



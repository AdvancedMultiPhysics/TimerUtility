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
[N_procs,timer,memory] = load_timer_file(file,glob);

% Compress the data if necessary and create the struct
s = whos('timer');
if s.bytes > 100e6
    % Reduce the size of timer
    for i = 1:length(timer)
        for j = 1:length(timer(i).trace)
            timer(i).trace(j).start = single(timer(i).trace(j).start);
            timer(i).trace(j).stop  = single(timer(i).trace(j).stop);
        end
    end
end
s = whos('memory');
for i = 1:length(memory)
    memory(i).scale = 1;    
    if s.bytes > 100e6
        % Reduce the size of memory
        memory(i).time = single(memory(i).time);
        mem_max = max(memory(i).bytes);
        while mem_max/memory(i).scale > 4e9
            memory(i).scale = 2*memory(i).scale;
        end
        memory(i).bytes = uint32(memory(i).bytes/memory(i).scale);
    end
end
data.N_procs = N_procs;
data.timer = timer;
data.memory = memory;
clear('N_procs','timer','memory','s');

% Remove empty timers and count the number of threads
N_threads = 0;
index = false(size(data.timer));
for i = 1:length(data.timer)
    N_threads = max([N_threads [data.timer(i).trace.thread]]);
    if isempty(data.timer(i).trace)
        index(i) = true;
    end
end
N_threads = N_threads+1;
data.timer(index) = [];

% Combine the trace info for the processes/threads
N_procs = data.N_procs;
for k = 1:length(data.timer)
    if ~any(strcmp(data.timer(k).id,{data.timer(k).trace.id}))
        error('Internal error');
    end
    trace = data.timer(k).trace;
    trace2 = struct([]);
    while ~isempty(trace)
        tmp.active = trace(1).active;
        tmp.N = zeros(N_threads,N_procs);
        tmp.min = 1e100*ones(N_threads,N_procs);
        tmp.max = zeros(N_threads,N_procs);
        tmp.tot = zeros(N_threads,N_procs);
        tmp.start = cell(N_threads,N_procs);
        tmp.stop = cell(N_threads,N_procs);
        index = find_trace_active( trace, tmp.active );
        i = [trace(index).thread]+1;
        j = [trace(index).rank]+1;
        index2 = i + (j-1)*N_threads;
        tmp.N(index2) = [trace(index).N];
        tmp.min(index2) = [trace(index).min];
        tmp.max(index2) = [trace(index).max];
        tmp.tot(index2) = [trace(index).tot];
        tmp.start(index2) = {trace(index).start};
        tmp.stop(index2) = {trace(index).stop};
        trace(index) = [];
        trace2 = [trace2 tmp]; %#ok<AGROW>
    end
    data.timer(k).trace = trace2;
end

% Change the ids of the timers from character strings to numbers
% All processors use the same id, but it is an alpha-numeric id.
map = {data.timer.id};
if length(unique(map)) ~= length(data.timer)
    error('Internal error');
end
for i = 1:length(data.timer)
    data.timer(i).id = i;
    for j = 1:length(data.timer(i).trace)
        active = data.timer(i).trace(j).active;
        active2 = zeros(size(active));
        for k = 1:length(active)
            active2(k) = find(strcmp(active{k},map));
        end
        data.timer(i).trace(j).active = sort(active2);
    end
    if any(any(~cellfun(@isempty,[data.timer(i).trace.start])))
        data.trace_data = true;
    end
end



function index = find_trace_active( trace, active )
index = [];
for i = 1:length(trace)
    if length(active)~=length(trace(i).active)
        continue;
    end
    if all(strcmp(active,trace(i).active))
        index = [index i]; %#ok<AGROW>
    end
end


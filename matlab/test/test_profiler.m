profile_mex('ENABLE');
profile_mex('ENABLE_TRACE',true);
profile_mex('ENABLE_MEMORY',true);
profile_mex('START','timer1');
profile_mex('START','timer2');
pause(1)
profile_mex('STOP','timer2');
profile_mex('STOP','timer1');
profile_mex('SAVE','matlab');
profile_mex('DISABLE');
[N_procs,timers,memory] = load_timer_file('matlab',false);

% Check that we can load the data
if numel(N_procs) ~= 1
    error('Error loading N_procs');
end
if numel(timers) ~= 2
    error('Error loading timers');
end
if isempty(memory)
    error('Error loading memory');
end

% Check the timer
if timers(1).id ~= 1 || ~strcmp(timers(1).message,'timer1') || ~strcmp(timers(1).file,'test_profiler.m') || numel(timers(1).trace) ~= 1
    timers(1)
    error('timer1 does not match');
end
if timers(2).id ~= 2 || ~strcmp(timers(2).message,'timer2') || ~strcmp(timers(2).file,'test_profiler.m') || numel(timers(2).trace) ~= 1
    timers(2)
    error('timer2 does not match');
end
if timers(2).trace.active ~= 1
    error('timer2.trace.active does not match');
end

fprintf(1,'All tests passed\n')



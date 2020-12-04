% Structure used for a trace structure
classdef TraceClass
    properties
        id      % Index of the parent timer
        N       % Number of calls to the trace (Nt x Np)
        min     % Minimum time for the trace (Nt x Np)
        max     % Maximum time for the trace (Nt x Np)
        tot     % Total time for the trace (Nt x Np)
        active  % List of the active timers for the trace
        start   % Start time for the trace
        stop    % Stop time for the trace
    end
    methods
        function x = TraceClass(N)
            if nargin ~= 0
                x(1,N) = TraceClass;
                for i = 1:N
                end
            end
        end
    end
end




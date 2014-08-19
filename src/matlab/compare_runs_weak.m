addpath('/projects/TimerUtility/build/debug/gui');
src_path = '/projects/AMP/scaling/070814/weak_scaling/titan/';
files = { ...
    '1x/inputMultiPellet348_1x_timer/inputMultiPellet348_1x'; ...
    '2x/inputMultiPellet348_2x_timer/inputMultiPellet348_2x'; ...
    '4x/inputMultiPellet348_4x_timer/inputMultiPellet348_4x' };

timers = { 
    'solve'      'solve'        'PetscSNESSolver.cc'           []  ; ...
    'global'     'apply'        'CoupledOperator.cc'           1   ; ...
    'diffusion'  'apply'        'NonlinearFEOperator.cc'       1   ; ...
    'robin'      'apply'        'RobinVectorCorrection.cc'     1   ;...      
    'maps'       'apply'        'AsyncMapColumnOperator.cc'    1   ; ...
    'reset'      'resetOperator'    'TrilinosMLSolver.cc'      1   ; ...
    'load'       'Load meshes'  'testDriver.cc'                []  ; ...
    'save'       'writeFile'    'SiloIO.cc'                    []  };
N_it = [29 31 36];


time = zeros(length(files),size(timers,1));
N = zeros(length(files),size(timers,1));
N_procs = zeros(length(files),1);
for i = 1:length(files)
   data = load_timer_set([src_path files{i}]);
   N_procs(i) = data.N_procs;
   data = data.timer;
   keep = zeros(length(timers),1);
   for j = 1:length(timers)
       for k = 1:length(data)
           if strcmp(timers{j,2},data(k).message) && strcmp(timers{j,3},data(k).file)
               keep(j) = k;
           end
       end
   end
   if any(keep==0)
       error('Timer not found');
   end
   data = data(keep);
   id = [data.id];
   for j = 1:length(timers)
       tmp = zeros(size(data(j).trace(1).tot));
       for k = 1:length(data(j).trace)
           if length(intersect(data(j).trace(k).active,id(timers{j,4})))==length(timers{j,4})
               tmp = tmp + data(j).trace(k).tot;
           end
       end
       time(i,j) = mean(sum(tmp,1));
   end
end
fprintf(1,'%8s','procs');
fprintf(1,'%10s',timers{:,1});
fprintf(1,'\n');
for i = 1:length(files)
    fprintf(1,'%8i',N_procs(i));
    fprintf(1,'%10.2f',time(i,:));
    fprintf(1,'\n');
    fprintf(1,'%8s','');
    fprintf(1,'%10.2f',time(i,:)/N_it(i));
    fprintf(1,'\n');
end
fprintf(1,'\n');


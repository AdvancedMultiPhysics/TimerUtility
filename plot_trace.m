function varargout = plot_trace(varargin)
% PLOT_TRACE M-file for plot_trace.fig
%      PLOT_TRACE, by itself, creates a new PLOT_TRACE or raises the existing
%      singleton*.
%
%      H = PLOT_TRACE returns the handle to a new PLOT_TRACE or the handle to
%      the existing singleton*.
%
%      PLOT_TRACE('CALLBACK',hObject,eventData,handles,...) calls the local
%      function named CALLBACK in PLOT_TRACE.M with the given input arguments.
%
%      PLOT_TRACE('Property','Value',...) creates a new PLOT_TRACE or raises the
%      existing singleton*.  Starting from the left, property value pairs are
%      applied to the GUI before plot_trace_OpeningFcn gets called.  An
%      unrecognized property name or invalid value makes property application
%      stop.  All inputs are passed to plot_trace_OpeningFcn via varargin.
%
%      *See GUI Options on GUIDE's Tools menu.  Choose "GUI allows only one
%      instance to run (singleton)".
%
% See also: GUIDE, GUIDATA, GUIHANDLES

% Edit the above text to modify the response to help plot_trace

% Last Modified by GUIDE v2.5 30-Jan-2013 09:41:15

% Begin initialization code - DO NOT EDIT
gui_Singleton = 0;
gui_State = struct('gui_Name',       mfilename, ...
                   'gui_Singleton',  gui_Singleton, ...
                   'gui_OpeningFcn', @plot_trace_OpeningFcn, ...
                   'gui_OutputFcn',  @plot_trace_OutputFcn, ...
                   'gui_LayoutFcn',  [] , ...
                   'gui_Callback',   []);
if nargin && ischar(varargin{1})
    gui_State.gui_Callback = str2func(varargin{1});
end

if nargout
    [varargout{1:nargout}] = gui_mainfcn(gui_State, varargin{:});
else
    gui_mainfcn(gui_State, varargin{:});
end
% End initialization code - DO NOT EDIT


% --- Executes just before plot_trace is made visible.
function plot_trace_OpeningFcn(hObject, eventdata, handles, varargin) %#ok<INUSL>
% This function has no output args, see OutputFcn.
% hObject    handle to figure
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
% varargin   command line arguments to plot_trace (see VARARGIN)

% Choose default command line output for plot_trace
handles.output = hObject;

% Update handles structure
guidata(hObject, handles);

% UIWAIT makes plot_trace wait for user response (see UIRESUME)
% uiwait(handles.figure1);


% --- Executes during object creation, after setting all properties.
function processor_popup_CreateFcn(hObject, eventdata, handles) %#ok<DEFNU,INUSD>
if ispc && isequal(get(hObject,'BackgroundColor'), get(0,'defaultUicontrolBackgroundColor'))
    set(hObject,'BackgroundColor','white');
end

% --- Executes during object creation, after setting all properties.
function thread_popup_CreateFcn(hObject, eventdata, handles) %#ok<DEFNU,INUSD>
if ispc && isequal(get(hObject,'BackgroundColor'), get(0,'defaultUicontrolBackgroundColor'))
    set(hObject,'BackgroundColor','white');
end

% --- Executes during object creation, after setting all properties.
function resolution_CreateFcn(hObject, eventdata, handles) %#ok<DEFNU,INUSD>
if ispc && isequal(get(hObject,'BackgroundColor'), get(0,'defaultUicontrolBackgroundColor'))
    set(hObject,'BackgroundColor','white');
end

% --- Executes during object creation, after setting all properties.
function slider1_CreateFcn(hObject, eventdata, handles) %#ok<DEFNU,INUSD>
if isequal(get(hObject,'BackgroundColor'), get(0,'defaultUicontrolBackgroundColor'))
    set(hObject,'BackgroundColor',[.9 .9 .9]);
end



% --- Outputs from this function are returned to the command line.
function varargout = plot_trace_OutputFcn(hObject, eventdata, handles)  %#ok<INUSL>
% varargout  cell array for returning output args (see VARARGOUT);
% hObject    handle to figure
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)

% Get default command line output from handles structure
varargout{1} = handles.output;


% --- Callback to update data
function update_data_Callback(hObject, eventdata, handles) %#ok<DEFNU,INUSL>
% hObject    handle to plot_trace_button (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
timers = getappdata(handles.parent_figure,'timer');
handles.N_threads = size(timers(1).trace(1).tot,1);
handles.N_procs = size(timers(1).trace(1).tot,2);
guidata(hObject, handles);
clear('timers');
string_text = cell(handles.N_procs+1,1);
string_text{1} = 'All';
for i = 1:handles.N_procs
   string_text{i+1} = ['Proc ',num2str(i)];
end
set(handles.processor_popup,'String',string_text);
string_text = cell(handles.N_threads+1,1);
string_text{1} = 'All';
for i = 1:handles.N_threads
   string_text{i+1} = ['Thread ',num2str(i)];
end
set(handles.thread_popup,'String',string_text);
set(handles.processor_popup,'Value',1);
set(handles.thread_popup,'Value',1);
set(handles.slider1,'Min',-1,'Max',0,'Value',0,'SliderStep',[1,1000]);
figure1_ResizeFcn(hObject,[],handles);
dispay_data(handles)


% --- Executes when user attempts to close figure1.
function figure1_CloseRequestFcn(hObject, eventdata, handles) %#ok<DEFNU,INUSL>
% hObject    handle to figure1 (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
if isfield(handles,'parent_figure')
    data = guidata(handles.parent_figure);
    data = rmfield(data,'trace_figure');
    guidata(handles.parent_figure,data);
end
% Hint: delete(hObject) closes the figure
delete(hObject);



% --- Executes on selection change in processor_popup.
function processor_popup_Callback(hObject, eventdata, handles) %#ok<DEFNU,INUSL,INUSL>
% hObject    handle to processor_popup (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
dispay_data(handles)

 
% --- Executes on selection change in thread_popup.
function thread_popup_Callback(hObject, eventdata, handles) %#ok<DEFNU,INUSL>
% hObject    handle to thread_popup (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
dispay_data(handles)


% --- Update the timer data and plot
function dispay_data(handles)
% First get the trace data for the current thread and processor
processor = get(handles.processor_popup,'Value')-1;
thread = get(handles.thread_popup,'Value')-1;
if processor==0 
    processor = 1:handles.N_procs;
end
if thread==0 
    thread = 1:handles.N_threads;
end
N_blocks = length(processor)*length(thread);
timers = getappdata(handles.parent_figure,'timer');
if isempty(timers)
    error('Missing appdata');
end
% Combine the trace data to create the global start/stop times
if ~isfield(timers,'trace')
    error('We should not be ploting trace data without trace info');
end
start = 1e100;
stop = -1e100;
for i = 1:length(timers)
    timers(i).start = timers(i).trace(1).start;
    timers(i).stop  = timers(i).trace(1).stop;
    timers(i).tot   = timers(i).trace(1).tot;
    for j = 2:length(timers(i).trace)
        for k = 1:N_blocks
            timers(i).start{k} = [ timers(i).start{k}, timers(i).trace(j).start{k} ];
            timers(i).stop{k}  = [ timers(i).stop{k},  timers(i).trace(j).stop{k}  ];
            timers(i).tot = timers(i).tot + timers(i).trace(j).tot;
        end
    end
    for k = 1:N_blocks
        timers(i).start{k} = sort(timers(i).start{k});
        timers(i).stop{k}  = sort(timers(i).stop{k});
        start = min([ start timers(i).start{k} ]);
        stop  = max([ stop  timers(i).stop{k}  ]);
    end
    timers(i).tot3 = sum(sum(timers(i).tot));
end
timers = rmfield(timers,'trace');
handles.N_timers = length(timers);
handles.start_time = start;
handles.stop_time = stop;
if isfield(handles,'t_range')
    handles.t_range(1) = max(handles.t_range(1),handles.start_time);
    handles.t_range(2) = min(handles.t_range(2),handles.stop_time);
else
    handles.t_range = [start stop];
end
% Save the current list of timers
guidata(handles.figure1,handles);
update_plot(handles,timers)



% --- Update the data displayed
function update_plot(handles,timers)
start = handles.start_time;
stop = handles.stop_time;
N_timers = length(timers);
N_blocks = numel(timers(1).start);
% Get the range info from the lines
x_range = handles.t_range;
% Get a list of the timers that are non-zero for the time of interest
index = [];
for i=1:N_timers
    keep = false;
    for j = 1:N_blocks
        t_start = timers(i).start{j};
        t_stop = timers(i).stop{j};
        if any(t_start<x_range(2)&t_stop>x_range(1))
            keep = true;
        end
    end
    if keep
        index(length(index)+1) = i; %#ok<AGROW>
    end
end
N_timers2 = length(index);
% Get the info from the slider
N_display = 15;
if N_timers2 <= N_display
    N_display = N_timers2;
    set(handles.slider1,'Min',-1,'Max',0,'Value',0,'SliderStep',[1,1000]);
else
    y_range = N_timers2-N_display;
    value = get(handles.slider1,'Value');
    value = min(value,y_range);
    if get(handles.slider1,'Min')==-1 || get(handles.slider1,'Max')~=y_range
        value = y_range;
    end
    stepSize = [1/y_range N_display/(N_timers2-N_display)];
    set(handles.slider1,'Min',0,'Max',y_range,'Value',value,'SliderStep',stepSize);
end
start_range = get(handles.slider1,'Value');
index = index(start_range+(1:N_display));
dy = 1/N_blocks;
y1 = dy/2:dy:N_timers;
y2 = start_range + (dy/2:dy:N_display);
% Compute the data images
n = str2num(get(handles.resolution,'String')); %#ok<ST2NM>
n = round(n);
t1 = start:(stop-start)/(n-1):stop;
data1 = zeros(n,length(y1));
for i=1:N_timers
    for j = 1:N_blocks
        t_start = timers(i).start{j};
        t_stop = timers(i).stop{j};
        tmp = get_active_times(t1,t_start,t_stop);
        data1(tmp,(i-1)*N_blocks+j) = i;
    end
end
t2 = x_range(1):(x_range(2)-x_range(1))/(n-1):x_range(2);
data2 = zeros(n,length(y2));
for i=1:length(index)
    for j = 1:N_blocks
        t_start = timers(index(i)).start{j};
        t_stop = timers(index(i)).stop{j};
        tmp = get_active_times(t2,t_start,t_stop);
        data2(tmp,(i-1)*N_blocks+j) = index(i);
    end
end
map = [0 0 0; jet(N_timers)];
% Get the memory usage
memory = [];
if isappdata(handles.parent_figure,'memory')
    memory_data = getappdata(handles.parent_figure,'memory');
    processor = get(handles.processor_popup,'Value')-1;
    if processor==0
        processor = 1:length(memory_data);
    end
    for k = 1:length(processor)
        p = processor(k);
        i1 = max([find(memory_data(p).time<=x_range(1),1,'last'),1]);
        i2 = find(memory_data(p).time<=x_range(2),1,'last');
        N = ceil((i2-i1)/10000);                        % Limit the plot to 10000 points
        i = [i1:N:i2 i2];
        memory(k).time = reshape(memory_data(p).time(i),1,[]);  %#ok<AGROW>
        memory(k).size = reshape(memory_data(p).bytes(i),1,[]); %#ok<AGROW>
        memory(k).time(1) = x_range(1)-1e-12;           %#ok<AGROW>
        memory(k).time(length(i)) = x_range(2)+1e-12;   %#ok<AGROW>
        tmp = memory(k);
        i1 = 1:length(tmp.time);
        i2 = 1:length(tmp.time)-1;
        j1 = 2*(i1-1)+1;
        j2 = 2*i2;
        memory(k).time(j1) = tmp.time(i1);              %#ok<AGROW>
        memory(k).time(j2) = tmp.time(i2+1);            %#ok<AGROW>
        memory(k).size(j1) = tmp.size(i1);              %#ok<AGROW>
        memory(k).size(j2) = tmp.size(i2);              %#ok<AGROW>
        clear('tmp')
        memory(k).time = double(memory(k).time);           %#ok<AGROW>
        memory(k).size = memory_data(p).scale*double(memory(k).size); %#ok<AGROW>
    end
end
clear('data','tmp');
% Plot the results for the small window (all times, all timers)
set(0,'currentfigure',handles.figure1);
set(handles.figure1,'CurrentAxes',handles.small_plot);
cla(handles.small_plot);
imagesc(t1,y1,data1',[0 N_timers])
colormap(map)
axis tight
set(gca,'XTick',[],'YTick',[],'Ydir','normal')
hold on
handles.line(1) = line(x_range(1)+[0 0],[0 N_timers]);
handles.line(2) = line(x_range(2)+[0 0],[0 N_timers]);
set(handles.line,'Color',[1 1 1],'LineWidth',3.0,'ButtonDownFcn',@lineSelectionCallback)
guidata(handles.figure1,handles);
% Plot the results for the main window
buttonFnc = get(handles.main_plot,'ButtonDownFcn');
set(handles.figure1,'CurrentAxes',handles.main_plot);
cla(handles.main_plot);
y = 0.5/N_blocks:1/N_blocks:length(index);
h = imagesc(t2,y,data2',[0 N_timers]);
colormap(map)
set(h,'HitTest','off')
set(gca,'YTick',index,'YTickLabel',cell(1,N_timers),'YDir','normal')
pos = x_range(1) - (x_range(2)-x_range(1))/200;
for i = 1:length(index)
    label = {timers(index(i)).file;timers(index(i)).message};
    label = strrep(label,'_','\_');
    h = text(double(pos),i-0.5,label);
    set(h,'horizontalAlignment','right','units','normalized');
end
hold on
for i = 0:length(index)
    h = plot(x_range(1:2),[i i],'w');
    set(h,'HitTest','off')
end
hold off
set(handles.main_plot,'ButtonDownFcn',buttonFnc);
if isempty(memory)
    xlabel('Time (s)');
else
    set(gca,'XTickLabel',{})
end
% Plot the memory usage
if ~isempty(memory)
    max_size = 1;
    min_size = 1e12;
    for i = 1:length(memory)
        max_size = max(max_size,max(memory(1).size));
        min_size = min(min_size,min(memory(1).size));
    end
    if max_size < 1e6
       scale = 1/1024;
       label = 'Memory (kB)';
    elseif max_size < 1e9
       scale = 1/1024^2;
       label = 'Memory (MB)';
    else
       scale = 1/1024^3;
       label = 'Memory (GB)';
    end
    set(handles.figure1,'CurrentAxes',handles.memory_plot);
    cla(handles.memory_plot);
    hold on
    color = 'bgrcmyk';
    for i = 1:length(memory)
        c = color(mod(i-1,length(color))+1);
        plot(memory(i).time,scale*memory(i).size,c)
    end
    %axis([x_range(1) x_range(2) 0 scale*max_size])
    %axis([x_range(1) x_range(2) scale*(min_size+[0 2e4])])
    axis tight
    xlabel('Time (s)');
    ylabel(label);
end


function lineSelectionCallback(hObject, eventdata) %#ok<INUSD>
% First make sure the axes contining the line and the parent have
% consistent units (necessary for gpos to work properly)
type = get(gcbf,'SelectionType');
parent = get(hObject,'Parent');
set(parent,'units',get(gcf,'units'));
handles = guidata(hObject);
if hObject==handles.line(1)
    xdata = get(handles.line(2),'Xdata');
    range = [handles.start_time,xdata(1)];
elseif hObject==handles.line(2)
    xdata = get(handles.line(1),'Xdata');
    range = [xdata(1),handles.stop_time];
else
    error('Internal error');
end
if strcmp(type,'normal')
    set(gcf,'WindowButtonUpFcn',{@moveLineCallbackStop,hObject,range},...
        'WindowButtonMotionFcn',{@moveLineCallbackMotion,hObject,range});
end


function moveLineCallbackStop( hObject, eventdata, hline, range ) %#ok<INUSL>
set(gcf,'WindowButtonUpFcn',[],'WindowButtonMotionFcn',[]);
parent = get(hline,'Parent');
[x,y] = gpos(gcf,parent); %#ok<NASGU>
x = max(x,range(1));
x = min(x,range(2));
set(hline,'XData',[x x])
handles = guidata(hObject);
if hline==handles.line(1)
    handles.t_range(1) = x;
elseif hline==handles.line(2)
    handles.t_range(2) = x;
else
    error('Internal error');
end
if handles.t_range(1)==handles.t_range(2)
    handles.t_range(2) = handles.t_range(1)+1e-9;
end
guidata(hObject,handles);
dispay_data(handles)


function moveLineCallbackMotion( hObject, eventdata, hline, range ) %#ok<INUSL>
parent = get(hline,'Parent');
[x,y] = gpos(gcf,parent); %#ok<NASGU>
x = max(x,range(1));
x = min(x,range(2));
set(hline,'XData',[x x])


function plotSelectionCallback(hObject, eventdata) %#ok<DEFNU,INUSD>
% First make sure the axes contining the line and the parent have
% consistent units (necessary for gpos to work properly)
type = get(gcbf,'SelectionType');
parent = get(hObject,'Parent');
set(parent,'units',get(gcf,'units'));
handles = guidata(hObject);
if hObject==handles.line(1)
    xdata = get(handles.line(2),'Xdata');
    range = [handles.start_time,xdata(1)];
elseif hObject==handles.line(2)
    xdata = get(handles.line(1),'Xdata');
    range = [xdata(1),handles.stop_time];
else
    error('Internal error');
end
if strcmp(type,'normal')
    set(gcf,'WindowButtonUpFcn',{@moveLineCallbackStop,hObject,range},...
        'WindowButtonMotionFcn',{@moveLineCallbackMotion,hObject,range});
end


% --- Executes on slider movement.
function slider1_Callback(hObject, eventdata, handles) %#ok<DEFNU,INUSL>
% hObject    handle to slider1 (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
if get(hObject,'Max')==0
    return;
end
set(hObject,'Value',round(get(hObject,'Value')));
dispay_data(handles)



function [x,y] = gpos(h_figure,h_axes)
% GPOS Get current position of cusor and return its coordinates in axes with handle h_axes
% Note: This requires that the units of h_figure and h_axes are consistent
% h_figure - handle of specified figure
% h_axes - handle of specified axes
% [x,y]  - cursor coordinates in axes h_aexs
pos_axes_unitfig    = get(h_axes,'position');
width_axes_unitfig  = pos_axes_unitfig(3);
height_axes_unitfig = pos_axes_unitfig(4);
xDir_axes=get(h_axes,'XDir');
yDir_axes=get(h_axes,'YDir');
pos_cursor_unitfig = get( h_figure, 'currentpoint'); % [left bottom]
if strcmp(xDir_axes,'normal')
    left_origin_unitfig = pos_axes_unitfig(1);
    x_cursor2origin_unitfig = pos_cursor_unitfig(1) - left_origin_unitfig;
else
    left_origin_unitfig = pos_axes_unitfig(1) + width_axes_unitfig;
    x_cursor2origin_unitfig = -( pos_cursor_unitfig(1) - left_origin_unitfig );
end
if strcmp(yDir_axes,'normal')
    bottom_origin_unitfig     = pos_axes_unitfig(2);
    y_cursor2origin_unitfig = pos_cursor_unitfig(2) - bottom_origin_unitfig;
else
    bottom_origin_unitfig = pos_axes_unitfig(2) + height_axes_unitfig;
    y_cursor2origin_unitfig = -( pos_cursor_unitfig(2) - bottom_origin_unitfig );
end
xlim_axes=get(h_axes,'XLim');
width_axes_unitaxes=xlim_axes(2)-xlim_axes(1);
ylim_axes=get(h_axes,'YLim');
height_axes_unitaxes=ylim_axes(2)-ylim_axes(1);
x = xlim_axes(1) + x_cursor2origin_unitfig / width_axes_unitfig * width_axes_unitaxes;
y = ylim_axes(1) + y_cursor2origin_unitfig / height_axes_unitfig * height_axes_unitaxes;


% --- Executes when figure1 is resized.
function figure1_ResizeFcn(hObject, eventdata, handles) %#ok<INUSL>
% hObject    handle to figure1 (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
plot_memory = false;    % Are we going to include memory data
if isfield(handles,'parent_figure')
    if isappdata(handles.parent_figure,'N_proc')
        error('Parent figure does not contain appdata');
    end
    if isappdata(handles.parent_figure,'memory')
        plot_memory = true;
    end
end
set(gcf,'Units','Pixels');
fig_pos = get(hObject,'Position');
w = fig_pos(3);         % Width of the figure
h = fig_pos(4);         % Height of the figure
pos1 = h-105;           % Position between small_plot and main_plot
pos2 = h-35;            % Position between small_plot and text boxes
pos3 = 0;               % Position between memory_plot and main_plot
if plot_memory
    pos3 = 90;
end
set(handles.main_plot,'Units','Pixels','OuterPosition',[0 0 w pos1],...
    'Position',[150 pos3+48 w-150-40 pos1-pos3-50]);
set(handles.small_plot,'Units','Pixels','OuterPosition',[0 pos1 w pos2-pos1],...
    'Position',[10 pos1+10 w-20 pos2-pos1-10]);
set(handles.slider1,'Units','Pixels','Position',[w-30 pos3+50 20 pos1-pos3-50]);
set(handles.proc_text,'Units','Pixels','Position',[20 pos2+5 100 20]);
set(handles.processor_popup,'Units','Pixels','Position',[120 pos2+10 100 20]);
set(handles.thread_text,'Units','Pixels','Position',[250 pos2+5 100 20]);
set(handles.thread_popup,'Units','Pixels','Position',[350 pos2+10 100 20]);
set(handles.resolution_text,'Units','Pixels','Position',[500 pos2+5 100 20]);
set(handles.resolution,'Units','Pixels','Position',[600 pos2+10 100 20]);
set(handles.reset,'Units','Pixels','Position',[800 pos2+8 80 24]);
if plot_memory
    set(handles.memory_plot,'Visible','on');
    set(handles.memory_plot,'Units','Pixels','OuterPosition',[0 0 w pos3],...
        'Position',[150 42 w-150-40 pos3]);
else
    set(handles.memory_plot,'Visible','off');
end
1;


% --- Executes on mouse press over axes background.
function main_plot_ButtonDownFcn(hObject, eventdata, handles) %#ok<DEFNU,INUSL>
% hObject    handle to main_plot (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
type = get(gcbf,'SelectionType');
if strcmp(type,'normal')
    set(hObject,'units',get(gcf,'units'));
    [x,y] = gpos(gcf,hObject); %#ok<NASGU>
    hold on
    N_timers = handles.N_timers;
    h(1) = line([x x],[0 N_timers]);
    h(2) = line([x x],[0 N_timers]);
    set(h,'Color',[1 1 1]);
    if strcmp(type,'normal')
        set(gcf,'WindowButtonUpFcn',{@movePlotLineCallbackStop,hObject,h},...
            'WindowButtonMotionFcn',{@movePlotLineCallbackMotion,hObject,h});
    end
elseif strcmp(type,'alt')
    %cmenu = uicontextmenu;
    %item1 = uimenu(cmenu,'Label','Reset');
    %%item2 = uimenu(cmenu, 'Label', 'dotted', 'Callback', cb2);
    %%item3 = uimenu(cmenu, 'Label', 'solid', 'Callback', cb3);
    %set(gcf,'uicontextmenu',cmenu)
    %1;
    %delete(cmenu);
end

function movePlotLineCallbackStop( hObject, eventdata, hPlot, hline ) %#ok<INUSL>
set(gcf,'WindowButtonUpFcn',[],'WindowButtonMotionFcn',[]);
hold off
x1 = get(hline(1),'XData');
x2 = get(hline(2),'XData');
delete(hline);
t = sort([x1(1),x2(1)]);
if diff(t) < 0.02*diff(get(hPlot,'xlim'))
    return;
end
handles = guidata(hObject);
handles.t_range = t;
guidata(hObject,handles);
dispay_data(handles)


function movePlotLineCallbackMotion( hObject, eventdata, hPlot, hline ) %#ok<INUSL>
[x,y] = gpos(gcf,hPlot); %#ok<NASGU>
set(hline(2),'XData',[x x])


function resolution_Callback(hObject, eventdata, handles) %#ok<DEFNU,INUSL>
% hObject    handle to resolution (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
N = str2double(get(hObject,'String'));
if isnan(N) || round(N)~=N
    printf('Resolution must be an integer value');
end
if N<100 || N>10000
    printf('Resolution must be: 100 <= R <= 10000');
end
dispay_data(handles)


% --- Executes on button press in reset.
function reset_Callback(hObject, eventdata, handles) %#ok<DEFNU,INUSL>
% hObject    handle to reset (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
handles.t_range = [handles.start_time handles.stop_time];
guidata(hObject,handles);
dispay_data(handles)


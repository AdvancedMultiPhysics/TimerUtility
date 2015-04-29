function varargout = load_timer(varargin)
% LOAD_TIMER M-file for load_timer.fig
%      LOAD_TIMER, by itself, creates a new LOAD_TIMER or raises the existing
%      singleton*.
%
%      H = LOAD_TIMER returns the handle to a new LOAD_TIMER or the handle to
%      the existing singleton*.
%
%      LOAD_TIMER('CALLBACK',hObject,eventData,handles,...) calls the local
%      function named CALLBACK in LOAD_TIMER.M with the given input arguments.
%
%      LOAD_TIMER('Property','Value',...) creates a new LOAD_TIMER or raises the
%      existing singleton*.  Starting from the left, property value pairs are
%      applied to the GUI before load_timer_OpeningFcn gets called.  An
%      unrecognized property name or invalid value makes property application
%      stop.  All inputs are passed to load_timer_OpeningFcn via varargin.
%
%      *See GUI Options on GUIDE's Tools menu.  Choose "GUI allows only one
%      instance to run (singleton)".
%
% See also: GUIDE, GUIDATA, GUIHANDLES

% Edit the above text to modify the response to help load_timer

% Last Modified by GUIDE v2.5 24-Jun-2013 09:11:39

% Begin initialization code - DO NOT EDIT
gui_Singleton = 0;
gui_State = struct('gui_Name',       mfilename, ...
                   'gui_Singleton',  gui_Singleton, ...
                   'gui_OpeningFcn', @load_timer_OpeningFcn, ...
                   'gui_OutputFcn',  @load_timer_OutputFcn, ...
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


% --- Executes just before load_timer is made visible.
function load_timer_OpeningFcn(hObject, eventdata, handles, varargin) %#ok<INUSL>
% This function has no output args, see OutputFcn.
% hObject    handle to figure
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
% varargin   command line arguments to load_timer (see VARARGIN)
handles.output = hObject;
show_all = false;    % Special flag to show all objects (helps with layout)
set(handles.figure1,'Units','pixels')
set(handles.load_plot,'Position',[0.05 0.05 0.9 0.18])
set(handles.load_text,'Position',[0.05 0.230 0.3 0.025])
set(handles.timer_table,'Position',[0.05 0.26 0.9 0.67])
set(handles.timer_table,'Position',[0.05 0.05 0.9 0.88])
set(handles.function_text,'Position',[0.05 0.93 0.8 0.025])
set(handles.load,'Position',[0.05 0.96 0.07 0.028])
set(handles.reset,'Position',[0.13 0.96 0.07 0.028])
set(handles.select_proc_text,'Position',[0.22 0.955 0.07 0.028],'HorizontalAlignment','right')
set(handles.select_proc,'Position',[0.30 0.96 0.08 0.028])
set(handles.inclusive_exclusive,'Position',[0.46 0.96 0.12 0.028])
set(handles.subfunctions,'Position',[0.6 0.96 0.12 0.028])
set(handles.plot_trace_button,'Position',[0.74 0.96 0.12 0.028])
set(handles.back,'Position',[0.88 0.96 0.07 0.028])
set(handles.select_proc,'Visible','off');
set(handles.select_proc_text,'Visible','off');
set(handles.subfunctions,'Visible','on');
set(handles.plot_trace_button,'Visible','off');
set(handles.load_plot,'Visible','off');
set(handles.load_text,'Visible','off');
set(handles.inclusive_exclusive,'Visible','off');
set(handles.subfunctions,'String','Hide Subfunctions','Value',0,'Visible','on');
set(handles.back,'Visible','off');
if show_all
    names = fieldnames(handles); %#ok<UNRCH>
    for i = 1:length(names)
        set(getfield(handles,names{i}),'Visible','on');
    end
end
handles.last_load_plot_pos = [];
handles.load_plot_rectangle = 0;
handles.pathname = '';
handles.filename = '';
guidata(hObject, handles);
if isempty(which('get_active_times'))
    warning('get_active_times.cpp does not appear to be compiled, attempting to compile now'); %#ok<WNTAG>
    mex -O get_active_times.cpp
end


% --- Executes when user attempts to close figure1.
function figure1_CloseRequestFcn(hObject, eventdata, handles) %#ok<DEFNU,INUSL>
% hObject    handle to figure1 (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
if isfield(handles,'trace_figure')
    close(handles.trace_figure);
end
if isfield(handles,'figure1')
    h = handles.figure1;
    fields = fieldnames(getappdata(h));
    for i = 1:length(fields)
        rmappdata(h,fields{i});
    end
end
% Hint: delete(hObject) closes the figure
delete(hObject);


% --- Outputs from this function are returned to the command line.
function varargout = load_timer_OutputFcn(hObject, eventdata, handles)  %#ok<INUSL>
% varargout  cell array for returning output args (see VARARGOUT);
% hObject    handle to figure
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)

% Get default command line output from handles structure
varargout{1} = handles.output;


function timers = get_timers(handles)
% This function load the data for the function hierarchy selected
% Get the call hierarchy
id_list = [];
if isempty(handles.call)
    set(handles.function_text,'String','All Functions');
else
    for i = 1:size(handles.call,1)
        j = find( strcmp(handles.id_data.message,handles.call{i,1}) & ...
            strcmp(handles.id_data.filename,handles.call{i,2}) );
        if isempty(j)
            id_list = -1;
            break;
        elseif length(j)>1
            error('Internal error');
        end
        id_list = [id_list,handles.id_data.ids(j)]; %#ok<AGROW>
    end
    id_list = unique(id_list);
end
% Get the relavent timers
timers = getappdata(handles.figure1,'timer');
subfunctions = get(handles.subfunctions,'Value');
id_max = max([timers.id]);
% Keep only those traces that have all of the selected call hierarchy
for i = length(timers):-1:1
    trace = timers(i).trace;
    index = false(size(trace));
    for j = 1:length(trace)
        tmp = [timers(i).id trace(j).active];
        for k = 1:length(id_list)
            if all(tmp~=id_list(k))
                index(j) = true;
                break;
            end
        end
    end
    trace(index) = [];
    if isempty(trace)
        timers(i) = [];
    else
        timers(i).trace = trace;
    end
end
% Keep only the traces that are directly inherited from the current call hierarchy
if subfunctions==1
    ids = [timers.id];
    keep1 = false(size(timers));
    for i = 1:length(timers)
        keep2 = false(size(timers(i).trace));
        for j = 1:length(keep2)
            id2 = [timers(i).trace(j).active];
            tmp = zeros(1,id_max);
            tmp(ids) = 1;
            tmp(id2) = tmp(id2)+1;
            if sum(tmp==2)<=1
                keep2(j) = true;
            end
        end
        timers(i).trace = timers(i).trace(keep2);
        if any(keep2)
            keep1(i) = true;
        end
    end
    timers = timers(keep1);
end
% Update the timers to remove the time from sub-timers if necessary
for i = 1:length(timers)
    timers(i).trace(1).tot2 = timers(i).trace(1).tot;
    timers(i).N = timers(i).trace(1).N;
    timers(i).min = timers(i).trace(1).min;
    timers(i).max = timers(i).trace(1).max;
    timers(i).tot = timers(i).trace(1).tot;
    timers(i).tot2 = timers(i).trace(1).tot2;
    for j = 2:length(timers(i).trace)
        timers(i).trace(j).tot2 = timers(i).trace(j).tot;
        timers(i).N = timers(i).N + timers(i).trace(j).N;
        timers(i).min = min( timers(i).min, timers(i).trace(j).min );
        timers(i).max = max( timers(i).max, timers(i).trace(j).max );
        timers(i).tot = timers(i).tot + timers(i).trace(j).tot;
        timers(i).tot2 = timers(i).tot2 + timers(i).trace(j).tot2;
    end
end
if get(handles.inclusive_exclusive,'Value')==1
    tmp = zeros(length(timers),1);
    N_trace = zeros(length(timers),1);
    for i = 1:length(timers)
        N_trace(i) = length(timers(i).trace);
        for j = 1:N_trace(i)
            tmp(i,j) = length(timers(i).trace(j).active);
        end
    end
    for i1 = 1:length(timers)
        for j1 = 1:N_trace(i1)
            a1 = sort([timers(i1).id timers(i1).trace(j1).active]);
            [i2,j2] = find(tmp==length(a1));
            for k = 1:length(i2)
                a2 = timers(i2(k)).trace(j2(k)).active;
                if all(a1==a2)
                    tot = timers(i2(k)).trace(j2(k)).tot;
                    timers(i1).trace(j1).tot = timers(i1).trace(j1).tot - tot;
                    timers(i1).tot = timers(i1).tot - tot;
                end
            end
        end
    end
end


function display_data(hObject, handles)
% This function displays all of the data to the figure window
h = handles.figure1;
handles.last_load_plot_pos = [];
set(handles.figure1,'WindowButtonMotionFcn','');
if handles.load_plot_rectangle ~= 0
    delete(handles.load_plot_rectangle);
    handles.load_plot_rectangle = 0;
end
guidata(hObject,handles);
% Update the name of the figure
if ~isempty(handles.filename)
    set(handles.figure1,'Name',sprintf('load_timer - %s%s',handles.pathname,handles.filename));
end
% Load the data for the function selected
N_proc = getappdata(h,'N_procs');
if isempty(N_proc)
    error('Internal error with appdata')
end
timers = get_timers(handles); 
% Sum the information over the threads for each timer
for i = 1:length(timers)
    k = timers(i).N>0;
    N = sum(timers(i).N,1);
    tot_t = sum(timers(i).tot,1);
    tot_t2 = sum(timers(i).tot2,1);
    max_t = max(timers(i).max,[],1);
    thread = false(size(timers(i).N));
    thread(k) = true;
    tmp = timers(i).min;
    tmp(~k) = 1e100;
    min_t = min(tmp,[],1);
    min_t(min_t==1e100) = 0;
    timers(i).N = N;
    timers(i).min = min_t;
    timers(i).max = max_t;
    timers(i).tot = tot_t;
    timers(i).tot2 = tot_t2;
    timers(i).thread = thread;
end
% Create the timers to display in the table
total_time = zeros(length(timers),N_proc);
for i = 1:length(timers)
    total_time(i,:) = timers(i).tot;
    if get(handles.select_proc,'Value')==1
        % We want to average each timer
        timers(i).N = round(mean(timers(i).N));
        timers(i).min = mean(timers(i).min);
        timers(i).max = mean(timers(i).max);
        timers(i).tot = mean(timers(i).tot);
        timers(i).tot2 = mean(timers(i).tot2);
        timers(i).thread = find(any(timers(i).thread,2));
    elseif get(handles.select_proc,'Value')==2
        % We want to take the minimum value for each processor
        timers(i).N = round(min(timers(i).N));
        timers(i).min = min(timers(i).min);
        timers(i).max = min(timers(i).max);
        timers(i).tot = min(timers(i).tot);
        timers(i).tot2 = min(timers(i).tot2);
        timers(i).thread = find(any(timers(i).thread,2));
    elseif get(handles.select_proc,'Value')==3
        % We want to take the maximum value for each processor
        timers(i).N = round(max(timers(i).N));
        timers(i).min = max(timers(i).min);
        timers(i).max = max(timers(i).max);
        timers(i).tot = max(timers(i).tot);
        timers(i).tot2 = max(timers(i).tot2);
        timers(i).thread = find(any(timers(i).thread,2));
    else
        % We have selected a specific processor
        p = get(handles.select_proc,'Value')-3;
        timers(i).N = timers(i).N(p);
        timers(i).min = timers(i).min(p);
        timers(i).max = timers(i).max(p);
        timers(i).tot = timers(i).tot(p);
        timers(i).tot2 = timers(i).tot2(p);
        timers(i).thread = find(any(timers(i).thread,2));
    end
end
% Update the table
if isempty(handles.call)
    set(handles.function_text,'String','All Functions');
else
    text = handles.call{1,1};
    for i = 2:length(handles.call(:,1))
        text = sprintf('%s  ->  %s',text,handles.call{i,1});
    end
    set(handles.function_text,'String',text);
end
table_data = cell(length(timers),10);
percent_time = zeros(length(timers),1);
[tot_time,index] = max([timers.tot2]);
for i = 1:length(timers)
    table_data{i,1} = timers(i).message;
    table_data{i,2} = timers(i).file;
    if length(timers(i).thread)==1
        table_data{i,3} = sprintf('% 4i',timers(i).thread);
    else
        table_data{i,3} = 'multiple';
    end
    table_data{i,4} = sprintf('%6i',timers(i).start);
    table_data{i,5} = sprintf('%6i',timers(i).stop);
    table_data{i,6} = sprintf('%5i',timers(i).N);
    table_data{i,7} = sprintf('%0.3g',timers(i).min);
    table_data{i,8} = sprintf('%0.3g',timers(i).max);
    table_data{i,9} = sprintf('%0.4g',timers(i).tot);
    table_data{i,10} = sprintf('%7.2f',100*timers(i).tot/tot_time);
    percent_time(i) = timers(i).tot/tot_time;
end
% Sort the table data vy the time %
[~,I] = sort(percent_time,'descend');
table_data = table_data(I,:);
% Plot the data in the table
FontSize = 4;
FontName = 'Courier New';
for i = 1:size(table_data,1)
    %pre = sprintf('<html><table border=0 width=400 bgcolor=#FF0000><TR><TD><pre><font size="%i" color="%s" face="%s">',FontSize,'black',FontName);
    %post = '</font></pre></TD></TR> </table></html>';
    pre = sprintf('<html><pre><font size="%i" color="%s" face="%s">',FontSize,'black',FontName);
    post = '</font></pre></html>';
    for j = 1:size(table_data,2)
        tmp = strrep(table_data{i,j},'<','&lt;');
        tmp = strrep(tmp,'>','&gt;');
        table_data{i,j} = sprintf('%s%s%s',pre,tmp,post);
    end
end
% Update the table data
update_table_data(handles.timer_table,table_data);
% Plot the load balance
if N_proc > 1 
    total_time2 = total_time(index,:);
    myfun = get(handles.load_plot,'ButtonDownFcn');
    axes(handles.load_plot);
    if size(total_time2,2)*sqrt(size(total_time2,1)) <= 128
        bar(total_time2')
    elseif size(total_time2,1)==1
        plot(total_time2)
    else
        error('Not finished');
    end
    hold on
    plot([0 N_proc+1],[mean(total_time2(:)) mean(total_time2(:))],'r--')
    hold off
    set(handles.load_plot,'ButtonDownFcn',myfun);
    reset_zoom_Callback(hObject,[],handles);
end
% Force all children to evaluate the button callback routine
children = get(handles.load_plot,'Children');
for i = 1:length(children)
    set(children(i),'ButtonDownFcn',...
        @(hObject2,eventdata)load_timer('load_plot_ButtonDownFcn',hObject,eventdata,guidata(hObject)));
end
% Update the trace gui if used
if isfield(handles,'trace_figure') 
    plot_trace_button_Callback(hObject, [], handles)
end



% Helper function to update the data in the table
function update_table_data(handle,data)
% Get the handle to the java object (if possible)
jtable = [];
try %#ok<TRYNC>
    jscrollpane = findjobj(handle);
    jtable = jscrollpane.getViewport.getView;
    set(jtable.getColumnModel().getColumn(0),'MaxWidth',0)
end
% Get the current column widths
ColumnWidth = get(handle,'ColumnWidth');
if ~isempty(jtable)
    for i = 1:length(ColumnWidth)
        ColumnWidth{i} = get(jtable.getColumnModel().getColumn(i-1),'Width');
    end
end
ColumnWidth{1} = -1;
% Update the table data
data2 = [cell(size(data,1),1) data];
for i = 1:size(data,1)
    data2{i,1} = i;
end
set(handle,'data',data2,'FontSize',11,'ColumnWidth',ColumnWidth);
% Turn the JIDE sorting on for the timer_table
if ~isempty(jtable)
    jtable.setSortable(true);		% or: set(jtable,'Sortable','on');
    jtable.setAutoResort(true);
    jtable.setMultiColumnSortable(true);
    jtable.setPreserveSelectionsAfterSorting(false);
    drawnow('update') % Force updates before setting column width
    jtable.getColumnModel().getColumn(0).setWidth(0);
    jtable.getColumnModel().getColumn(0).setMaxWidth(0);
    jtable.getColumnModel().getColumn(0).setPreferredWidth(0);
end
drawnow     % Ensure table has been drawn



% --- Executes on button press in load.
function load_Callback(hObject, eventdata, handles) %#ok<INUSL,DEFNU>
% hObject    handle to load (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
if isfield(handles,'pathname')
    initial_dir = handles.pathname;
else
    initial_dir = pwd;
end
[FileName,PathName] = uigetfile({'*.0.timer;*.1.timer','Timer Files'},'Select the timer file',initial_dir);
if FileName == 0
    return;
end
n = length(FileName);
if ~strcmp(FileName(max(n-5,1):n),'.timer')
    fprintf(1,'Not a valid filename\n');
    return;
end
% Load all of the timer data
h = handles.figure1;
fields = fieldnames(getappdata(h));
for i = 1:length(fields)
    rmappdata(h,fields{i});
end
data = load_timer_set([PathName,FileName]);
if isfield(data,'N_procs')
    setappdata(h,'N_procs',data.N_procs)
end
if isfield(data,'timer')
    setappdata(h,'timer',data.timer)
end
if isfield(data,'trace_data')
    setappdata(h,'trace_data',data.trace_data)
end
if isfield(data,'memory')
    if ~isempty(data.memory)
        setappdata(h,'memory',data.memory)
    end
end
% Get the list of unique timer ids
N_procs = data.N_procs;
id_data.ids = [data.timer.id];
id_data.filename = {data.timer.file};
id_data.message = {data.timer.message};
[id_data.ids,I,J] = unique(id_data.ids); %#ok<NASGU>
id_data.filename = id_data.filename(I);
id_data.message = id_data.message(I);
clear('data');
% Set the timer selection button
if N_procs > 1
    string_text{1,1} = 'Average';
    string_text{2,1} = 'Minimum';
    string_text{3,1} = 'Maximum';
    for i = 1:N_procs
        string_text{i+3,1} = ['Proc ',num2str(i)];
    end
    set(handles.select_proc,'String',string_text);
    set(handles.select_proc,'Visible','on');
    set(handles.select_proc_text,'Visible','on');
    set(handles.load_plot,'Visible','on');
    set(handles.load_text,'Visible','on');
    set(handles.load_plot,'Position',[0.05 0.05 0.9 0.18])
    set(handles.load_text,'Position',[0.05 0.230 0.3 0.025])
    set(handles.timer_table,'Position',[0.05 0.26 0.9 0.67])
else
    set(handles.select_proc,'Visible','off');
    set(handles.select_proc_text,'Visible','off');
    set(handles.load_plot,'Visible','off');
    set(handles.load_text,'Visible','off');
    set(handles.timer_table,'Position',[0.05 0.05 0.9 0.88])
end
if isappdata(h,'trace_data')
    set(handles.plot_trace_button,'Visible','on');
else
    set(handles.plot_trace_button,'Visible','off');
end
set(handles.inclusive_exclusive,'String','Exclusive Time','Value',0,'Visible','on');
handles.id_data = id_data;
handles.call = [];
handles.pathname = PathName;
handles.filename = FileName;
guidata(hObject,handles);
reset_Callback(handles.reset,[],handles);


% --- Executes on button press in reset.
function reset_Callback(hObject, eventdata, handles) %#ok<INUSL>
% hObject    handle to reset (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
handles.call = [];
set(handles.subfunctions,'String','Hide Subfunctions','Value',0,'Visible','on');
set(handles.inclusive_exclusive,'String','Exclusive Time','Value',0,'Visible','on');
set(handles.back,'Visible','off');
guidata(hObject,handles);
display_data(hObject,handles);


% --- Executes when selected cell(s) is changed in timer_table.
function timer_table_CellSelectionCallback(hObject, eventdata, handles) %#ok<DEFNU>
% hObject    handle to timer_table (see GCBO)
% eventdata  structure with the following fields (see UITABLE)
%	Indices: row and column indices of the cell(s) currently selecteds
% handles    structure with handles and user data (see GUIDATA)
data = get(handles.timer_table,'Data');
indices = eventdata.Indices;
for i = 1:size(indices,1)
    indices(i,1) = data{indices(i,1)};
end
try %#ok<TRYNC>
    % Reset the selected cells
    jscrollpane = findjobj(handles.timer_table);
    jtable = jscrollpane.getViewport.getView;
    for i = 1:size(indices,1)
        indices(i,1) = str2double(jtable.getValueAt(eventdata.Indices(i,1)-1,0));
    end
    jtable.clearSelection();
end
if isempty(indices)
    % Nothing is selected
elseif indices(1,2) == 2
    % We want to dive into the function
    call{1,1} = strip_HTML(data{indices(1,1),2});
    call{1,2} = strip_HTML(data{indices(1,1),3});
    if isempty(call{1,1})
        return;
    end
    n = size(handles.call,1);
    if n==0
        handles.call = call;
    elseif ~strcmp(call{1},handles.call{n,1}) || ~strcmp(call{2},handles.call{n,2})
        handles.call = [handles.call; call];
    end
    set(handles.subfunctions,'Visible','on');
    set(handles.back,'Visible','on');
    guidata(hObject, handles);
    display_data(hObject,handles)
else
    % No options for this case
    1;
end


% --- Executes on selection change in select_proc.
function select_proc_Callback(hObject, eventdata, handles) %#ok<INUSL,DEFNU>
% hObject    handle to select_proc (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
display_data(hObject,handles)



% --- Executes during object creation, after setting all properties.
function select_proc_CreateFcn(hObject, eventdata, handles) %#ok<INUSD,DEFNU>
% hObject    handle to select_proc (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    empty - handles not created until after all CreateFcns called
if ispc && isequal(get(hObject,'BackgroundColor'), get(0,'defaultUicontrolBackgroundColor'))
    set(hObject,'BackgroundColor','white');
end


function data = strip_HTML(text)
% This function strips HTML fields from the given text
i1 = find(text=='<');
i2 = find(text=='>');
if length(i1) ~= length(i2)
    error('Unexpected data in string');
end
data = text;
for i = length(i1):-1:1
    data(i1(i):i2(i)) = [];
end
data = strrep(data,'&lt;','<');
data = strrep(data,'&gt;','>');


% --- Executes on button press in subfunctions.
function subfunctions_Callback(hObject, eventdata, handles) %#ok<INUSL,DEFNU>
% hObject    handle to subfunctions (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
if get(hObject,'Value')==1
    set(hObject,'String','Show Subfunctions');
else
    set(hObject,'String','Hide Subfunctions');
end
display_data(hObject,handles);


% --- Executes on button press in back.
function back_Callback(hObject, eventdata, handles) %#ok<DEFNU,INUSL>
% hObject    handle to back (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
n = size(handles.call,1);
if n>0
    handles.call = handles.call(1:n-1,:);
end
guidata(hObject, handles);
display_data(hObject,handles)


% --- Executes on mouse press over axes background.
function load_plot_ButtonDownFcn(hObject, eventdata, handles) %#ok<DEFNU,INUSL>
% hObject    handle to load_plot (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
switch get(handles.figure1,'SelectionType')
    case 'normal'
        % We want to zoom
        if isempty(handles.last_load_plot_pos)
            pos = get(handles.load_plot,'CurrentPoint');
            pos = pos(1,1:2);
            handles.last_load_plot_pos = pos;
            handles.load_plot_rectangle = rectangle('Position',[pos(1) pos(2) 1e-6 1e-6]);
            guidata(hObject, handles);
            children = get(handles.load_plot,'Children');
            for i = 1:length(children)
                set(children(i),'ButtonDownFcn',...
                    @(hObject2,eventdata)load_timer('load_plot_ButtonDownFcn',hObject,eventdata,guidata(hObject)));
            end
            set(handles.figure1,'WindowButtonMotionFcn',...
                @(hObject,eventdata)load_timer('move_box',hObject,eventdata,guidata(hObject)));
        else
            last_pos = handles.last_load_plot_pos;
            handles.last_load_plot_pos = [];
            set(handles.figure1,'WindowButtonMotionFcn','');
            if handles.load_plot_rectangle ~= 0
                delete(handles.load_plot_rectangle);
                handles.load_plot_rectangle = 0;
            end
            guidata(hObject, handles);
            pos = get(handles.load_plot,'CurrentPoint');
            pos = pos(1,1:2);
            xlim(1) = min(pos(1),last_pos(1));
            xlim(2) = max(pos(1),last_pos(1));
            ylim(1) = min(pos(2),last_pos(2));
            ylim(2) = max(pos(2),last_pos(2));
            set(handles.load_plot,'XLim',xlim,'YLim',ylim);
        end
    case 'alt'
        % We want to bring up the menu
        handles.last_load_plot_pos = [];
        set(handles.figure1,'WindowButtonMotionFcn','');
        if handles.load_plot_rectangle ~= 0
            delete(handles.load_plot_rectangle);
            handles.load_plot_rectangle = 0;
        end
        guidata(hObject, handles);
        root = get(handles.figure1,'Parent');
        position1 = get(root,'PointerLocation');
        position2 = get(handles.figure1,'Position');
        position1 = position1 - position2(1:2);
        set(handles.load_plot_menu,'Position',position1);
        set(handles.load_plot_menu,'Visible','on');
    otherwise
        % Unknown click
        handles.last_load_plot_pos = [];
        set(handles.figure1,'WindowButtonMotionFcn','');
        if handles.load_plot_rectangle ~= 0
            delete(handles.load_plot_rectangle);
            handles.load_plot_rectangle = 0;
        end
        guidata(hObject, handles);
end


% --- Executes on mouse movement
function move_box(hObject, eventdata, handles) %#ok<DEFNU,INUSL>
% hObject    handle to load_plot (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
root = get(handles.figure1,'Parent');
position1 = get(root,'PointerLocation');
position2 = get(handles.figure1,'Position');
position3 = get(handles.load_plot,'Position');
position(1) = (position1(1)-position2(1))/position2(3);
position(2) = (position1(2)-position2(2))/position2(4);
position(1) = (position(1)-position3(1))/position3(3);
position(2) = (position(2)-position3(2))/position3(4);
if position(1)>=0 && position(1)<=1 && position(2)>=0 && position(2)<=1
    xlim = get(handles.load_plot,'XLim');
    ylim = get(handles.load_plot,'YLim');
    position(1) = xlim(1)+diff(xlim)*position(1);
    position(2) = ylim(1)+diff(ylim)*position(2);
    start = min(handles.last_load_plot_pos,position);
    range = max(handles.last_load_plot_pos,position)-start;
    set(handles.load_plot_rectangle,'Position',[start,range]);
end


% --------------------------------------------------------------------
function load_plot_menu_Callback(hObject, eventdata, handles) %#ok<DEFNU,INUSD>
% hObject    handle to load_plot_menu (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)



% --------------------------------------------------------------------
function reset_zoom_Callback(hObject, eventdata, handles) %#ok<INUSL>
% hObject    handle to reset_zoom (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
handles.last_load_plot_pos = [];
set(handles.figure1,'WindowButtonMotionFcn','');
if handles.load_plot_rectangle ~= 0
    delete(handles.load_plot_rectangle);
    handles.load_plot_rectangle = 0;
end
children = get(handles.load_plot,'Children');
N_procs = 0;
max_val = 0;
for i = 1:length(children)
    xdata = get(children(i),'XData');
    ydata = get(children(i),'YData');
    N_procs = max(N_procs,max(xdata));
    max_val = max(max_val,max(ydata));
end
set(handles.load_plot,'XLim',[0 N_procs+1]);
set(handles.load_plot,'YLim',[0 1.05*max_val+1]);
guidata(hObject, handles);


% --------------------------------------------------------------------
function export_load_balance_Callback(hObject, eventdata, handles) %#ok<DEFNU,INUSL>
% hObject    handle to export_load_balance (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
children = get(handles.load_plot,'Children');
figure;
h = axes;
copyobj(children,h)
xlabel('Processor');
ylabel('Time (s)');
title(get(handles.function_text,'String'));


% --- Executes on button press in plot_trace_button.
function plot_trace_button_Callback(hObject, eventdata, handles) %#ok<INUSL>
% hObject    handle to plot_trace_button (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
if ~isfield(handles,'trace_figure')
    handles.trace_figure = plot_trace;
    guidata(hObject, handles);
end
data = guidata(handles.trace_figure);
data.parent_figure = handles.figure1;
if isfield(data,'t_range')
    data = rmfield(data,'t_range');
end
plot_trace('update_data_Callback',handles.trace_figure,[],data);


% --- Executes on button press in inclusive_exclusive.
function inclusive_exclusive_Callback(hObject, eventdata, handles) %#ok<DEFNU,INUSL>
% hObject    handle to inclusive_exclusive (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
if get(hObject,'Value')==0
    set(hObject,'String','Exclusive Time');
else
    set(hObject,'String','Inclusive Time');
end
display_data(hObject,handles)



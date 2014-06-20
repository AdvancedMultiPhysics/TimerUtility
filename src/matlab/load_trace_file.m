function data = load_trace_file(file)

data = struct([]);
fid = fopen(file);
i = 1;
while 1
    pos = ftell(fid);
    tline = fgetl(fid);
    if ~ischar(tline), break, end
    j = find(tline==':',1);
    tline = tline(1:j);
    i1 = strfind(tline,'id=');
    i2 = strfind(tline,'thread=');
    i3 = strfind(tline,'active=');
    i4 = strfind(tline,'N=');
    i0 = find(tline==','|tline==':');
    data(i).id = tline(i1+3:i0(find(i0>i1+3,1))-1);
    data(i).thread = str2double(tline(i2+7:i0(find(i0>i2,1))-1));
    active = tline(i3+7:i0(find(i0>i3,1))-1);
    data(i).N = str2double(tline(i4+2:i0(find(i0>i4,1))-1));
    active = strtrim(active(2:length(active)-1));
    if isempty(active)
        data(i).active = {};
    else
        index = [0 find(active==' ') length(active)+1];
        N = length(index)-1;
        data(i).active = cell(1,N);
        for k=1:N
            data(i).active{k} = active(index(k)+1:index(k+1)-1);
        end
    end
    fseek(fid,pos+length(tline),'bof');
    data(i).start = fread(fid,data(i).N,'double');
    data(i).end = fread(fid,data(i).N,'double');
    fseek(fid,1,'cof');
    i = i+1;
end
fclose(fid);


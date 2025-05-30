function [mseDP, mseEMT] = compareDpAndEmt(filenameRef, filenameVoltageDP, filenameVoltageEMT, plotNode)

% Increment node to skip time column
plotNode = plotNode + 1;

% Read from CSV files
voltageRef = csvread(filenameRef);
voltageDP = csvread(filenameVoltageDP);
compOffsetDP = (size(voltageDP,2) - 1) / 2;
voltageEMT = csvread(filenameVoltageEMT);

% Calculate DP absolute value
voltageAbsDP = voltageDP(:,1);
for col = 2:( compOffsetDP + 1 )
    for row = 1:size(voltageDP,1)
        voltageAbsDP(row,col) = sqrt(voltageDP(row,col)^2 + ...
            voltageDP(row,col+compOffsetDP)^2);
    end
end

% Shift DP values
voltageShiftDP = voltageDP(:,1);
for col = 2:(compOffsetDP + 1)
    for row = 1:size(voltageDP,1)
        voltageShiftDP(row,col) = voltageDP(row,col)*cos(2*pi*50*voltageDP(row,1)) - ...
            voltageDP(row,col+compOffsetDP)*sin(2*pi*50*voltageDP(row,1));
    end
end

% Downsampling of reference voltage
for row = 1:size(voltageShiftDP,1)
    if voltageRef(size(voltageRef,1),1) >= voltageShiftDP(row,1)
        indices(row) = find(voltageRef(:,1) == voltageShiftDP(row,1),1);
    end
end
for i = 1:size(indices,2)
    row = indices(1,i);
    for col = 1:size(voltageRef,2)
        voltageRefAligned(i,col) = voltageRef(row,col);
        voltageShiftDPAligned(i,col) = voltageShiftDP(i,col);
        voltageEMTAligned(i,col) = voltageEMT(i,col);
    end
end

% Calculate MSEs
mseDP = sum((voltageRefAligned - voltageShiftDPAligned).^2) / size(voltageRefAligned,1)
mseEMT = sum((voltageRefAligned - voltageEMTAligned).^2) / size(voltageRefAligned,1)

% Plot
figure1 = figure('Name',['DP EMT Comparison ' num2str(voltageDP(2,1))],'NumberTitle','off');
axes1 = axes('Parent',figure1);
hold(axes1,'on');

EMTplot = plot(voltageEMT(:,1),voltageEMT(:,plotNode), 'b--');
DPplot = plot(voltageShiftDP(:,1),voltageShiftDP(:,plotNode), 'r-.');
DPabsPlot = plot(voltageAbsDP(:,1),voltageAbsDP(:,plotNode), 'k-');
RefPlot = plot(voltageRef(:,1),voltageRef(:,plotNode), 'm:');

legend('EMT', 'DP shift', 'DP abs', 'Ref')
xlabel('time [s]')
ylabel('voltage [V]')

end

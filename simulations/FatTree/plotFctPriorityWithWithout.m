clc,clear ,close all
conf=dlmread(fullfile('with/MatConfig.csv'));
k = conf(16);
numShortFlows=conf(2); 

 
conf=dlmread(fullfile('with/MatConfig.csv'));
percentLongFlowNodes=conf(4); 
kBytes=conf(3)*1500/1000;

%%
numServers= (k^3)/4;
numlongflowsRunningServers = floor(numServers * percentLongFlowNodes); 

nLines = length(kBytes);

for i=1:length(kBytes)
    a= num2str(kBytes(i));
    b=num2str(numServers);
    c=dlmread(fullfile('with/MatInstThroughput.csv'));
    r=sort(c);
    shortFlows= r(1:end-numlongflowsRunningServers);
     figure,
     plot(shortFlows, 'r') 
     hold on
end


for i=1:length(kBytes)
    a= num2str(kBytes(i));
    b=num2str(numServers);
    c=dlmread(fullfile('without/MatInstThroughput.csv'));
    r=sort(c);
    shortFlows= r(1:end-numlongflowsRunningServers);
    hold on,
    plot(shortFlows, 'b') 
     hold on
end

imTitle = ['Web search '];
set(gca,'fontsize',13)
title(imTitle );
xlabel('flow rank', 'FontSize',13)
ylabel('Goodput' ,'FontSize',13)
grid
 ax = gca;
 ax.YAxis.Exponent = 9;
 ax.XAxis.Exponent = 0;
ylim([0 1*10^9])





%% FCT  with

fct=dlmread(fullfile('with/aaMatFct.csv'));
fct(find(fct<0)) = 0;
BYT=dlmread(fullfile('with/aaMatFlowSizes.csv'));
    

BytVsFct = [BYT , fct];
BytVsFct(find(BytVsFct(:,1)==0),:) = [];
BytVsFctSorted = sortrows(BytVsFct);
BytVsFctSorted(find(BytVsFctSorted(:,2)==0),:) = [];

q0_10k_with = BytVsFctSorted((BytVsFctSorted(:,1) > 0) & (BytVsFctSorted(:,1) <= 10000),2) ;
q10k_100k_with = BytVsFctSorted((BytVsFctSorted(:,1) > 10000) & (BytVsFctSorted(:,1) <= 100000),2) ;
q100k_1M_with = BytVsFctSorted((BytVsFctSorted(:,1) > 100000) & (BytVsFctSorted(:,1) <= 10^6),2) ;
q1M_10M_with = BytVsFctSorted((BytVsFctSorted(:,1) > 10^6) & (BytVsFctSorted(:,1) <= 10*10^6),2) ;


all_fct_with = [q0_10k_with;q10k_100k_with;q100k_1M_with;q1M_10M_with];
q0_100k_with =[q0_10k_with;q10k_100k_with];


BytVsFctSortedWith = BytVsFctSorted;


%% FCT  withouttttttttttttt
fct=dlmread(fullfile('without/aaMatFct.csv'));
fct(find(fct<0)) = 0;
BYT=dlmread(fullfile('without/aaMatFlowSizes.csv'));
    
BytVsFct = [BYT , fct];
BytVsFct(find(BytVsFct(:,1)==0),:) = [];
BytVsFctSorted = sortrows(BytVsFct);
BytVsFctSortedWithout = BytVsFctSorted;

BytVsFctSorted(find(BytVsFctSorted(:,2)==0),:) = [];
q0_10k_without = BytVsFctSorted((BytVsFctSorted(:,1) > 0) & (BytVsFctSorted(:,1) <= 10000),2) ;
q10k_100k_without = BytVsFctSorted((BytVsFctSorted(:,1) > 10000) & (BytVsFctSorted(:,1) <= 100000),2) ;
q100k_1M_without = BytVsFctSorted((BytVsFctSorted(:,1) > 100000) & (BytVsFctSorted(:,1) <= 10^6),2) ;
q1M_10M_without = BytVsFctSorted((BytVsFctSorted(:,1) > 10^6) & (BytVsFctSorted(:,1) <= 10*10^6),2) ;

all_fct_without = [q0_10k_without;q10k_100k_without;q100k_1M_without;q1M_10M_without];
q0_100k_without =[q0_10k_without;q10k_100k_without];

 
 
%%
figure,
h = cdfplot(q0_10k_without);
grid  , set(h,'linewidth',2); set(h,'Color','b');
hold on
h = cdfplot(q0_10k_with); 
grid  , set(h,'linewidth',2); set(h,'Color','r');
imTitle = {'web search: 0-10K'};
legend(['without priority'],['with priority'])
set(gca,'fontsize',13) ,title(imTitle);
%  xlim([0 0.0004])
ylim([0 1.2]) , ylabel('CDF', 'FontSize',13) , xlabel('FCT (sec)' ,'FontSize',13)
grid, ax = gca; ax.YAxis.Exponent = 0; ax.XAxis.Exponent = 0;
 set(gca,'XScale','log');
 

figure,
h = cdfplot(q10k_100k_without);
grid  , set(h,'linewidth',2); set(h,'Color','b');
hold on
h = cdfplot(q10k_100k_with); 
grid  , set(h,'linewidth',2); set(h,'Color','r');
imTitle = {'web search: 10k-100K'};
legend(['without priority'],['with priority'])
set(gca,'fontsize',13) ,title(imTitle);
%  xlim([0 0.0004])
ylim([0 1.2]) , ylabel('CDF', 'FontSize',13) , xlabel('FCT (sec)' ,'FontSize',13)
grid, ax = gca; ax.YAxis.Exponent = 0; ax.XAxis.Exponent = 0;




figure,
h = cdfplot(q100k_1M_without);
grid  , set(h,'linewidth',2); set(h,'Color','b');
hold on
h = cdfplot(q100k_1M_with); 
grid  , set(h,'linewidth',2); set(h,'Color','r');
imTitle = {'web search: 100k-1M'};
legend(['without priority'],['with priority'])
set(gca,'fontsize',13) ,title(imTitle);
%  xlim([0 0.0004])
ylim([0 1.2]) , ylabel('CDF', 'FontSize',13) , xlabel('FCT (sec)' ,'FontSize',13)
grid, ax = gca; ax.YAxis.Exponent = 0; ax.XAxis.Exponent = 0;



figure,
h = cdfplot(q1M_10M_without);
grid  , set(h,'linewidth',2); set(h,'Color','b');
hold on
h = cdfplot(q1M_10M_with); 
grid  , set(h,'linewidth',2); set(h,'Color','r');
imTitle = {'web search: 1M-10M'};
legend(['without priority'],['with priority'])
set(gca,'fontsize',13) ,title(imTitle);
%  xlim([0 0.0004])
ylim([0 1.2]) , ylabel('CDF', 'FontSize',13) , xlabel('FCT (sec)' ,'FontSize',13)
grid, ax = gca; ax.YAxis.Exponent = 0; ax.XAxis.Exponent = 0;



figure,
h = cdfplot(q0_100k_without);
grid  , set(h,'linewidth',2); set(h,'Color','b');
hold on
h = cdfplot(q0_100k_with); 
grid  , set(h,'linewidth',2); set(h,'Color','r');
imTitle = {'web search: 0-100K'};
legend(['without priority'],['with priority'])
set(gca,'fontsize',13) ,title(imTitle);
%  xlim([0 0.0004])
ylim([0 1.2]) , ylabel('CDF', 'FontSize',13) , xlabel('FCT (sec)' ,'FontSize',13)
grid, ax = gca; ax.YAxis.Exponent = 0; ax.XAxis.Exponent = 0;


figure,
h = cdfplot(all_fct_without);
grid  , set(h,'linewidth',2); set(h,'Color','b');
hold on
h = cdfplot(all_fct_with); 
grid  , set(h,'linewidth',2); set(h,'Color','r');
imTitle = {'web search: all'};
legend(['without priority'],['with priority'])
set(gca,'fontsize',13) ,title(imTitle);
%  xlim([0 0.0004])
ylim([0 1.2]) , ylabel('CDF', 'FontSize',13) , xlabel('FCT (sec)' ,'FontSize',13)
grid, ax = gca; ax.YAxis.Exponent = 0; ax.XAxis.Exponent = 0;

%%
average_fct_with_0_6 = [mean(q0_10k_with),mean(q10k_100k_with),mean(q100k_1M_with),mean(q1M_10M_with)];
average_fct_without_0_6 = [mean(q0_10k_without),mean(q10k_100k_without),mean(q100k_1M_without),mean(q1M_10M_without)];

save('avg_fct_0_6.mat','average_fct_with_0_6','average_fct_without_0_6')

% set(gca,'XScale','log');

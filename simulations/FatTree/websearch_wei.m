clc,clear,close all

cdf_m = [0.001
    0.07
    0.1
    0.12
    0.13
    0.15
    0.16
    0.17
    0.18
    0.19
    0.3
    0.38
    0.45
    0.51
    0.55
    0.57
    0.58
    0.59
    0.6
    0.65
    0.66
    0.67
    0.68
    0.69
    0.7
    0.72
    0.74
    0.76
    0.88
    0.9
    0.92
    0.93
    0.96
    0.97]


size_m =[1500
    2000
    3000
    4000
    5000
    6000
    7000
    8000
    9000
    10000
    20000
    30000
    40000
    50000
    60000
    70000
    80000
    90000
    100000
    200000
    300000
    400000
    500000
    600000
    700000
    800000
    900000
    1000000
    2000000
    3000000
    4000000
    5000000
    6000000
    7000000
    8000000]

cdf_wei =[ 0
    0.15
    0.2
    0.3
    0.4
    0.53
    0.6
    0.7
    0.8
    0.9
    0.97
    1];


size_wei = [6
    6
    13
    19
    33
    53
    133
    667
    1333
    3333
    6667
    20000]*10^3;


%%  wei
seed = rand;
rng(seed)
for i=1:1000000
    randomnum = rand;
    select =  find(cdf_wei>randomnum) ;
    websearch_bytes(i) = size_wei(select(1));
end

figure, h = cdfplot(websearch_bytes); 
set(gca,'XScale','log'); title('web search wei')
xlim([0 10*10^6]) 
% histogram(websearch_bytes,200)
% figure, stem(websearch_bytes)



%%  mooo
for i=1:10000
    randomnum = rand;
    while randomnum>0.97
        randomnum = rand;
    end
    select =  find(cdf_m>randomnum) ;
    websearch_bytes_m(i) = size_m(select(1));
end
hold on, h = cdfplot(websearch_bytes_m); 
 
set(h,'linewidth',2); 
set(h,'Color','r');
set(gca,'XScale','log'); 

xlim([0 10*10^6])

legend(['wei'],['mo'])




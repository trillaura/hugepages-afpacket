#!/usr/bin
DAT=../plot__synt.dat
awk '$1 == "HUGETLB"' $DAT > plot_25.dat
awk '$1 == "TRANSHUGE"' $DAT > plot_26.dat
awk '$1 == "GFPNORETRY"' $DAT > plot_27.dat
awk '$1 == "VZALLOC"' $DAT > plot_28.dat
awk '$1 == "GFPRETRY"' $DAT > plot_29.dat
awk '$1 == "RX_RING"' $DAT > plot_5.dat
gnuplot plot.gplot
#cp *std*.png ../../../thesis/imgs
#cp *std*.png ../../../slides/thesis/pictures

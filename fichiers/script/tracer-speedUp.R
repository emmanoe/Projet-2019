#!/usr/bin/env Rscript

## plot speed up curves
## arguments are filenames - lines of data files must be "#threads time" format
## the last argument is the reference time measure

library(Hmisc) # contains errbar

args = commandArgs(trailingOnly=TRUE)


nbFichiers = length(args) - 1

if (nbFichiers < 1) {
  stop("pas assez d'arguments : il faut une liste de fichiers suivie par le temps de référence", call.=FALSE)
}


refTime = as.numeric(args[nbFichiers+1])

tables <- vector(mode = "list", length = nbFichiers)
sdtables <- vector(mode = "list", length = nbFichiers) #sd = standard deviation
xmax = 0
ymax = 0


for(i in 1:nbFichiers)
    {
        tmp = read.table(args[i])
        tmp[,2] = refTime / tmp[,2]  # compute speed up 
        tables[[i]] = aggregate(tmp[,2], tmp[1], mean)
        sdtmp = aggregate(tmp[,2], tmp[1], sd)
	sdtmp[is.na(sdtmp)] <- 0
	sdtables[[i]] = sdtmp
        xmax = max(max(tables[[i]][,1]),xmax)
        ymax = max(max(tables[[i]][,2]+ sdtables[[i]][,2]),ymax)
    }


pdf("speedup.pdf")


plot(1,type='n',xlim=c(0,xmax),ylim=c(0,ymax),xlab='#threads', ylab='speedup')

legend("topleft", legend = args[1:nbFichiers], col=1:nbFichiers, pch=1)

title(main=paste("Speedup (reference time = " ,  args[nbFichiers+1],")"))


for (i in 1:nbFichiers){
    lines(tables[[i]][,1],tables[[i]][,2], type='o', col=i, lwd=2)
    par(fg=i)
    errbar( tables[[i]][,1],tables[[i]][,2],
            tables[[i]][,2]+sdtables[[i]][,2],
            tables[[i]][,2]-sdtables[[i]][,2],
            col=i,add=TRUE)
}

dev.off()

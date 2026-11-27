
(function(root){root.mr2kernel=function (st,inL,inR,outL,outR,len) {
"use strict";
const sat24=(v)=>v>0x7fffff?0x7fffff:(v<-0x800000?-0x800000:v);
const se24=(v)=>(v<<8)>>8; const se30=(v)=>(v<<2)>>2;
const ram=st.ram, reg=st.reg, rs=st.rs, eram=st.eram, C=st.C;
let pos=st.pos, epos=st.epos, Aa=st.Aa, Ba=st.Ba, latch=st.latch, wl=st.wl, varOff=st.varOff, armed=st.armed, lastx=st.lastx, lastB=st.lastB;
let ea0,ea1,ea2,ea3,ea4,ea5,ea6,ea7,ea8,ea9;
const hook=st.hook;
let x=0,mulb=0,fl=0,r=0,rep=0,dB=0,t=0,A_e2=0,B_e3=0,B_e2=0,A_e3=0,B_e5=0,A_e10=0,B_e13=0,B_e12=0,A_e12=0,A_e13=0,B_e15=0,B_e17=0,B_e22=0,B_e23=0,A_e25=0,B_e26=0,B_e28=0,A_e28=0,B_e33=0,A_e33=0,B_e34=0,A_e34=0,B_e37=0,B_e38=0,A_e38=0,B_e39=0,A_e39=0,B_e40=0,A_e40=0,B_e41=0,A_e41=0,B_e44=0,B_e47=0,A_e49=0,B_e52=0,B_e51=0,A_e51=0,A_e52=0,A_e55=0,B_e55=0,B_e56=0,A_e56=0,B_e57=0,B_e59=0,A_e59=0,A_e61=0,B_e62=0,B_e66=0,B_e68=0,B_e70=0,B_e72=0,B_e79=0,B_e81=0,B_e82=0,A_e84=0,B_e84=0,B_e85=0,A_e85=0,B_e86=0,A_e86=0,A_e89=0,A_e92=0,B_e95=0,A_e95=0,B_e97=0,A_e99=0,B_e101=0,A_e105=0,A_e106=0,A_e112=0,B_e113=0,B_e112=0,A_e113=0,B_e115=0,A_e120=0,B_e123=0,B_e122=0,A_e122=0,A_e123=0,B_e125=0,B_e127=0,B_e132=0,B_e133=0,A_e135=0,B_e136=0,B_e138=0,A_e138=0,B_e143=0,A_e143=0,B_e144=0,A_e144=0,B_e145=0,A_e145=0,B_e146=0,A_e146=0,B_e147=0,B_e148=0,A_e148=0,B_e149=0,A_e149=0,A_e152=0,A_e155=0,B_e157=0,A_e160=0,B_e160=0,B_e161=0,A_e161=0,B_e162=0,A_e162=0,B_e163=0,A_e163=0,B_e164=0,B_e166=0,A_e166=0,A_e168=0,B_e169=0,B_e174=0,B_e177=0,A_e177=0,B_e180=0,B_e185=0,B_e187=0,A_e189=0,B_e190=0,A_e190=0,B_e191=0,A_e191=0,A_e194=0,A_e197=0,B_e198=0,A_e198=0,B_e199=0,A_e199=0,B_e200=0,A_e200=0,B_e202=0,A_e204=0,B_e210=0,B_e212=0,B_e215=0,B_e214=0,A_e214=0,A_e215=0,B_e219=0,B_e223=0,B_e225=0,A_e225=0,B_e226=0,B_e234=0,B_e245=0,A_e248=0,B_e250=0,B_e254=0,A_e254=0,A_e256=0,B_e258=0,B_e259=0,A_e259=0,A_e261=0,B_e262=0,A_e264=0,B_e265=0,A_e268=0,B_e269=0,A_e273=0,B_e275=0,A_e282=0,B_e282=0,A_e283=0,B_e283=0,A_e284=0,B_e284=0,A_e285=0,B_e285=0,A_m0n=0,B_m0n=0,A_m1n=0,B_m1n=0,A_m2n=0,B_m2n=0,A_m3n=0,B_m3n=0,A_q0n=0,B_q0n=0,A_q1n=0,B_q1n=0,A_q2n=0,B_q2n=0,A_q3n=0,B_q3n=0;
let A_m0=st.H[0],B_m0=st.H[1];let A_m1=st.H[2],B_m1=st.H[3];let A_m2=st.H[4],B_m2=st.H[5];let A_m3=st.H[6],B_m3=st.H[7];let A_q0=st.H[8],B_q0=st.H[9];let A_q1=st.H[10],B_q1=st.H[11];let A_q2=st.H[12],B_q2=st.H[13];let A_q3=st.H[14],B_q3=st.H[15];

for(let n=0;n<len;n++){
 ram[(pos+0x38)&127]=inL[n]; ram[(pos+0x39)&127]=inR[n];
 if(st.pre)st.pre(inL[n],inR[n]);
 let oL=0,oR=0;
 // slot 0
x=4194304;mulb=C[0];Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>6));lastx=x;lastB=0;
// slot 1
x=lastx>>7;if(lastB)Ba=se30(Ba+((se24(x)*(C[1]&0x7f))>>6));else Aa=se30(Aa+((se24(x)*(C[1]&0x7f))>>6));lastx=x;
// slot 2
x=ram[((pos+62)&127)];mulb=C[2];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;A_e2=Aa;B_e2=Ba;
// slot 3
x=ram[((pos+56)&127)];mulb=C[3];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>5));lastx=x;lastB=1;A_e3=Aa;B_e3=Ba;
// slot 4
t=sat24(A_e2);if(t!==reg[4])rs[4]=0;reg[4]=t;lastx=0;lastB=0;
// slot 5
t=sat24(B_e3);reg[0]=t<0?0:t;rs[0]=1;lastx=0;lastB=0;B_e5=Ba;
// slot 6
fl=C[6]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e2):sat24(A_e2);ram[((pos+6)&127)]=x;}else x=ram[((pos+6)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;
// slot 7
fl=C[7]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e3):sat24(A_e3);ram[((pos+0)&127)]=x;}else x=ram[((pos+0)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;
// slot 8
ea0=epos+32768;
wl=sat24(B_e5);if(armed){t=0x7fffff-reg[0];if(t!==reg[3])rs[3]=0;reg[3]=t;}lastx=0;lastB=0;
// slot 9
x=65536;mulb=C[9];Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));lastx=x;lastB=0;
// slot 10
x=lastx>>7;if(lastB)Ba=se30(Ba+((se24(x)*(C[10]&0x7f))>>7));else Aa=se30(Aa+((se24(x)*(C[10]&0x7f))>>7));lastx=x;A_e10=Aa;
// slot 11
eram[ea0&262143]=wl;
x=lastx>>7;if(lastB)Ba=se30(Ba+((se24(x)*(C[11]&0x7f))>>3));else Aa=se30(Aa+((se24(x)*(C[11]&0x7f))>>3));lastx=x;
// slot 12
x=65536;mulb=C[12];Ba=se30(Ba+((se24(x)*mulb)>>5));lastx=x;lastB=1;A_e12=Aa;B_e12=Ba;
// slot 13
x=sat24(A_e10);ram[((pos+65)&127)]=x;mulb=C[13];Aa=se30(Aa+((se24(x)*mulb)>>7));lastx=x;lastB=0;A_e13=Aa;B_e13=Ba;
// slot 14

// slot 15
t=sat24(B_e13);reg[0]=t<0?0:t;rs[0]=1;lastx=0;lastB=0;B_e15=Ba;
// slot 16
fl=C[16]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e12):sat24(A_e12);ram[((pos+6)&127)]=x;}else x=ram[((pos+6)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;
// slot 17
fl=C[17]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e13):sat24(A_e13);ram[((pos+0)&127)]=x;}else x=ram[((pos+0)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;B_e17=Ba;
// slot 18
x=sat24(B_e15);ram[((pos+66)&127)]=x;mulb=C[18];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;
// slot 19
x=16;mulb=C[19];Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));lastx=x;lastB=0;
// slot 20
x=sat24(B_e17);ram[((pos+6)&127)]=x;mulb=C[20];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>5));lastx=x;lastB=1;
// slot 21
x=lastx>>7;if(lastB)Ba=se30(Ba+((se24(x)*(C[21]&0x7f))>>5));else Aa=se30(Aa+((se24(x)*(C[21]&0x7f))>>5));lastx=x;
// slot 22
x=ram[((pos+66)&127)];mulb=C[22];Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;B_e22=Ba;
// slot 23
x=ram[((pos+62)&127)];mulb=C[23];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;B_e23=Ba;
// slot 24
x=lastx>>7;if(lastB)Ba=se30(Ba+((se24(x)*(C[24]&0x7f))>>7));else Aa=se30(Aa+((se24(x)*(C[24]&0x7f))>>7));lastx=x;
// slot 25
t=sat24(B_e22);reg[0]=t<0?0:t;rs[0]=1;lastx=0;lastB=0;A_e25=Aa;
// slot 26
x=ram[((pos+66)&127)];mulb=C[26];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;B_e26=Ba;
// slot 27
x=sat24(B_e23);ram[((pos+6)&127)]=x;mulb=C[27];Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;
// slot 28
ea1=epos+32800;
x=sat24(A_e25);ram[((pos+7)&127)]=x;mulb=C[28];Aa=se30(Aa+((se24(x)*mulb)>>7));lastx=x;lastB=0;A_e28=Aa;B_e28=Ba;
// slot 29
x=sat24(B_e26);ram[((pos+66)&127)]=x;mulb=C[29];Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;
// slot 30
x=ram[((pos+66)&127)];mulb=C[30];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;
// slot 31
latch=eram[ea1&262143];
fl=C[31]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e28):sat24(A_e28);ram[((pos+7)&127)]=x;}else x=ram[((pos+7)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>6));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>6));}lastx=x;lastB=dB;
// slot 32

// slot 33
x=ram[((pos+6)&127)];mulb=C[33];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;A_e33=Aa;B_e33=Ba;
// slot 34
x=ram[((pos+62)&127)];mulb=C[34];Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;A_e34=Aa;B_e34=Ba;
// slot 35
x=ram[((pos+65)&127)];mulb=C[35];Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;
// slot 36
fl=C[36]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e33):sat24(A_e33);ram[((pos+62)&127)]=x;}else x=ram[((pos+62)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;
// slot 37
fl=C[37]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e34):sat24(A_e34);ram[((pos+0)&127)]=x;}else x=ram[((pos+0)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;B_e37=Ba;
// slot 38
A_e38=Aa;B_e38=Ba;
// slot 39
A_e39=Aa;B_e39=Ba;
// slot 40
x=sat24(B_e37);if(x<0)x=0;ram[((pos+61)&127)]=x;Ba=se30(Ba+((se24(x)*C[40])>>7));lastx=x;lastB=1;A_e40=Aa;B_e40=Ba;
// slot 41
fl=C[41]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e38):sat24(A_e38);ram[((pos+64)&127)]=x;}else x=ram[((pos+64)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;A_e41=Aa;B_e41=Ba;
// slot 42
fl=C[42]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e39):sat24(A_e39);ram[((pos+0)&127)]=x;}else x=ram[((pos+0)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;
// slot 43
fl=C[43]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e40):sat24(A_e40);ram[((pos+59)&127)]=x;}else x=ram[((pos+59)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;
// slot 44
fl=C[44]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e41):sat24(A_e41);ram[((pos+0)&127)]=x;}else x=ram[((pos+0)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;B_e44=Ba;
// slot 45
x=4194304;mulb=C[45];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;
// slot 46
ram[((pos+126)&127)]=latch;x=latch;Ba=se30(Ba+((se24(x)*(C[46]))>>7));lastx=x;lastB=1;
// slot 47
x=sat24(B_e44);ram[((pos+63)&127)]=x;mulb=C[47];Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;B_e47=Ba;
// slot 48
x=ram[((pos+121)&127)];mulb=C[48];Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>6));lastx=x;lastB=0;
// slot 49
x=4194304;mulb=C[49];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>5));lastx=x;lastB=1;A_e49=Aa;
// slot 50
x=sat24(B_e47);ram[((pos+6)&127)]=x;mulb=C[50];Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;
// slot 51
x=lastx>>7;if(lastB)Ba=se30(Ba+((se24(x)*(C[51]&0x7f))>>6));else Aa=se30(Aa+((se24(x)*(C[51]&0x7f))>>6));lastx=x;A_e51=Aa;B_e51=Ba;
// slot 52
t=sat24(A_e49);if(t!==reg[1])rs[1]=0;reg[1]=t;lastx=0;lastB=0;A_e52=Aa;B_e52=Ba;
// slot 53
x=4194304;mulb=C[53];Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>6));lastx=x;lastB=0;
// slot 54
t=sat24(B_e52);reg[0]=t<0?0:t;rs[0]=1;lastx=0;lastB=0;
if(hook)reg[0]=hook(54);
// slot 55
fl=C[55]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e51):sat24(A_e51);ram[((pos+126)&127)]=x;}else x=ram[((pos+126)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;A_e55=Aa;B_e55=Ba;
// slot 56
fl=C[56]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e52):sat24(A_e52);ram[((pos+0)&127)]=x;}else x=ram[((pos+0)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;A_e56=Aa;B_e56=Ba;
// slot 57
t=sat24(A_e55);if(t!==reg[4])rs[4]=0;reg[4]=t;lastx=0;lastB=0;B_e57=Ba;
// slot 58
fl=C[58]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e55):sat24(A_e55);ram[((pos+109)&127)]=x;}else x=ram[((pos+109)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;
// slot 59
fl=C[59]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e56):sat24(A_e56);ram[((pos+0)&127)]=x;}else x=ram[((pos+0)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;A_e59=Aa;B_e59=Ba;
// slot 60
x=sat24(B_e57);ram[((pos+6)&127)]=x;mulb=C[60];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>3));lastx=x;lastB=1;
// slot 61
x=lastx>>7;if(lastB)Ba=se30(Ba+((se24(x)*(C[61]&0x7f))>>3));else Aa=se30(Aa+((se24(x)*(C[61]&0x7f))>>3));lastx=x;A_e61=Aa;
// slot 62
fl=C[62]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e59):sat24(A_e59);ram[((pos+96)&127)]=x;}else x=ram[((pos+96)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;B_e62=Ba;
// slot 63
ea2=epos+0;
x=1024;mulb=C[63];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;
// slot 64
armed=1;varOff=A_e61;t=(varOff&1023)<<13;if(t!==reg[0])rs[0]=0;reg[0]=t;lastx=0;lastB=0;
// slot 65
wl=sat24(B_e62);if(armed){t=0x7fffff-reg[0];if(t!==reg[3])rs[3]=0;reg[3]=t;}lastx=0;lastB=0;
// slot 66
eram[ea2&262143]=wl;
x=ram[((pos+75)&127)];mulb=C[66];Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;B_e66=Ba;
// slot 67
armed=0;
x=1024;mulb=C[67];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;
// slot 68
x=ram[((pos+77)&127)];mulb=C[68];Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;B_e68=Ba;
// slot 69
x=se24(B_e66);ram[((pos+74)&127)]=x;Ba=se30((se24(x)*C[69])>>7);lastx=x;lastB=1;
// slot 70
x=ram[((pos+75)&127)];mulb=C[70];Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;B_e70=Ba;
// slot 71
x=se24(B_e68);ram[((pos+76)&127)]=x;Ba=se30((se24(x)*C[71])>>7);lastx=x;lastB=1;
// slot 72
ea3=epos+(varOff>>10)+0;
x=ram[((pos+77)&127)];mulb=C[72];Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;B_e72=Ba;
// slot 73
x=sat24(B_e70);ram[((pos+80)&127)]=x;mulb=C[73];Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;
// slot 74

// slot 75
latch=eram[ea3&262143];
x=sat24(B_e72);ram[((pos+81)&127)]=x;mulb=C[75];Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;
// slot 76
armed=0;
x=ram[((pos+0)&127)];mulb=C[76];Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;
// slot 77
ram[((pos+126)&127)]=latch;x=latch;Ba=se30(Ba+((se24(x)*(C[77]))>>7));lastx=x;lastB=1;
// slot 78
x=1024;mulb=C[78];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;
// slot 79
x=ram[((pos+84)&127)];mulb=C[79];Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;B_e79=Ba;
// slot 80
x=1024;mulb=C[80];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;
// slot 81
ea4=epos+(varOff>>10)+1;
x=ram[((pos+86)&127)];mulb=C[81];Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;B_e81=Ba;
// slot 82
x=se24(B_e79);ram[((pos+83)&127)]=x;Ba=se30((se24(x)*C[82])>>6);lastx=x;lastB=1;B_e82=Ba;
// slot 83
x=ram[((pos+119)&127)];mulb=C[83];Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>6));lastx=x;lastB=0;
// slot 84
latch=eram[ea4&262143];
x=se24(B_e81);ram[((pos+85)&127)]=x;Ba=se30((se24(x)*C[84])>>6);lastx=x;lastB=1;A_e84=Aa;B_e84=Ba;
// slot 85
x=sat24(B_e82);ram[((pos+87)&127)]=x;mulb=C[85];Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;A_e85=Aa;B_e85=Ba;
// slot 86
t=sat24(A_e84);if(t!==reg[4])rs[4]=0;reg[4]=t;lastx=0;lastB=0;A_e86=Aa;B_e86=Ba;
// slot 87
x=sat24(B_e84);ram[((pos+88)&127)]=x;mulb=C[87];Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;
// slot 88
fl=C[88]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e85):sat24(A_e85);ram[((pos+127)&127)]=x;}else x=latch;mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;
// slot 89
fl=C[89]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e86):sat24(A_e86);ram[((pos+126)&127)]=x;}else x=ram[((pos+126)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;A_e89=Aa;
// slot 90
x=ram[((pos+112)&127)];mulb=C[90];Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));lastx=x;lastB=0;
// slot 91
x=ram[((pos+111)&127)];mulb=C[91];Aa=se30(Aa+((se24(x)*mulb)>>7));lastx=x;lastB=0;
// slot 92
x=sat24(A_e89);ram[((pos+110)&127)]=x;mulb=C[92];Aa=se30(Aa+((se24(x)*mulb)>>7));lastx=x;lastB=0;A_e92=Aa;
// slot 93
x=ram[((pos+112)&127)];mulb=C[93];Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));lastx=x;lastB=0;
// slot 94
x=ram[((pos+113)&127)];mulb=C[94];Aa=se30(Aa+((se24(x)*mulb)>>7));lastx=x;lastB=0;
// slot 95
x=sat24(A_e92);ram[((pos+111)&127)]=x;mulb=C[95];Aa=se30(Aa+((se24(x)*mulb)>>6));lastx=x;lastB=0;A_e95=Aa;B_e95=Ba;
// slot 96
x=1024;mulb=C[96];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;
// slot 97
x=ram[((pos+79)&127)];mulb=C[97];Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;B_e97=Ba;
// slot 98
fl=C[98]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e95):sat24(A_e95);ram[((pos+112)&127)]=x;}else x=ram[((pos+112)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;
// slot 99
A_e99=Aa;
// slot 100
x=se24(B_e97);ram[((pos+78)&127)]=x;Ba=se30((se24(x)*C[100])>>7);lastx=x;lastB=1;
// slot 101
x=ram[((pos+79)&127)];mulb=C[101];Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;B_e101=Ba;
// slot 102
x=sat24(A_e99);ram[((pos+29)&127)]=x;mulb=C[102];Aa=se30(Aa+((se24(x)*mulb)>>7));lastx=x;lastB=0;
oL=ram[((pos+29)&127)];
// slot 103

// slot 104
x=sat24(B_e101);ram[((pos+82)&127)]=x;mulb=C[104];Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;
// slot 105
lastx=0;lastB=0;A_e105=Aa;
// slot 106
lastx=0;lastB=0;A_e106=Aa;
// slot 107

// slot 108
x=sat24(A_e105);ram[((pos+15)&127)]=x;mulb=C[108];Aa=se30(Aa+((se24(x)*mulb)>>7));lastx=x;lastB=0;
// slot 109
x=sat24(A_e106);ram[((pos+19)&127)]=x;mulb=C[109];Aa=se30(Aa+((se24(x)*mulb)>>7));lastx=x;lastB=0;
// slot 110
x=4194304;mulb=C[110];Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>6));lastx=x;lastB=0;
// slot 111
x=lastx>>7;if(lastB)Ba=se30(Ba+((se24(x)*(C[111]&0x7f))>>6));else Aa=se30(Aa+((se24(x)*(C[111]&0x7f))>>6));lastx=x;
// slot 112
x=ram[((pos+69)&127)];mulb=C[112];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;A_e112=Aa;B_e112=Ba;
// slot 113
x=ram[((pos+57)&127)];mulb=C[113];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>5));lastx=x;lastB=1;A_e113=Aa;B_e113=Ba;
// slot 114
t=sat24(A_e112);if(t!==reg[4])rs[4]=0;reg[4]=t;lastx=0;lastB=0;
// slot 115
t=sat24(B_e113);reg[0]=t<0?0:t;rs[0]=1;lastx=0;lastB=0;B_e115=Ba;
// slot 116
fl=C[116]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e112):sat24(A_e112);ram[((pos+6)&127)]=x;}else x=ram[((pos+6)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;
// slot 117
fl=C[117]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e113):sat24(A_e113);ram[((pos+0)&127)]=x;}else x=ram[((pos+0)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;
// slot 118
ea5=epos+35328;
wl=sat24(B_e115);if(armed){t=0x7fffff-reg[0];if(t!==reg[3])rs[3]=0;reg[3]=t;}lastx=0;lastB=0;
// slot 119
x=65536;mulb=C[119];Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));lastx=x;lastB=0;
// slot 120
x=lastx>>7;if(lastB)Ba=se30(Ba+((se24(x)*(C[120]&0x7f))>>7));else Aa=se30(Aa+((se24(x)*(C[120]&0x7f))>>7));lastx=x;A_e120=Aa;
// slot 121
eram[ea5&262143]=wl;
x=lastx>>7;if(lastB)Ba=se30(Ba+((se24(x)*(C[121]&0x7f))>>3));else Aa=se30(Aa+((se24(x)*(C[121]&0x7f))>>3));lastx=x;
// slot 122
x=65536;mulb=C[122];Ba=se30(Ba+((se24(x)*mulb)>>5));lastx=x;lastB=1;A_e122=Aa;B_e122=Ba;
// slot 123
x=sat24(A_e120);ram[((pos+72)&127)]=x;mulb=C[123];Aa=se30(Aa+((se24(x)*mulb)>>7));lastx=x;lastB=0;A_e123=Aa;B_e123=Ba;
// slot 124

// slot 125
t=sat24(B_e123);reg[0]=t<0?0:t;rs[0]=1;lastx=0;lastB=0;B_e125=Ba;
// slot 126
fl=C[126]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e122):sat24(A_e122);ram[((pos+6)&127)]=x;}else x=ram[((pos+6)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;
// slot 127
fl=C[127]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e123):sat24(A_e123);ram[((pos+0)&127)]=x;}else x=ram[((pos+0)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;B_e127=Ba;
// slot 128
x=sat24(B_e125);ram[((pos+73)&127)]=x;mulb=C[128];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;
// slot 129
x=16;mulb=C[129];Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));lastx=x;lastB=0;
// slot 130
x=sat24(B_e127);ram[((pos+6)&127)]=x;mulb=C[130];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>5));lastx=x;lastB=1;
// slot 131
x=lastx>>7;if(lastB)Ba=se30(Ba+((se24(x)*(C[131]&0x7f))>>5));else Aa=se30(Aa+((se24(x)*(C[131]&0x7f))>>5));lastx=x;
// slot 132
x=ram[((pos+73)&127)];mulb=C[132];Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;B_e132=Ba;
// slot 133
x=ram[((pos+69)&127)];mulb=C[133];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;B_e133=Ba;
// slot 134
x=lastx>>7;if(lastB)Ba=se30(Ba+((se24(x)*(C[134]&0x7f))>>7));else Aa=se30(Aa+((se24(x)*(C[134]&0x7f))>>7));lastx=x;
// slot 135
t=sat24(B_e132);reg[0]=t<0?0:t;rs[0]=1;lastx=0;lastB=0;A_e135=Aa;
// slot 136
x=ram[((pos+73)&127)];mulb=C[136];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;B_e136=Ba;
// slot 137
x=sat24(B_e133);ram[((pos+6)&127)]=x;mulb=C[137];Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;
// slot 138
ea6=epos+35360;
x=sat24(A_e135);ram[((pos+7)&127)]=x;mulb=C[138];Aa=se30(Aa+((se24(x)*mulb)>>7));lastx=x;lastB=0;A_e138=Aa;B_e138=Ba;
// slot 139
x=sat24(B_e136);ram[((pos+73)&127)]=x;mulb=C[139];Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;
// slot 140
x=ram[((pos+73)&127)];mulb=C[140];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;
// slot 141
latch=eram[ea6&262143];
fl=C[141]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e138):sat24(A_e138);ram[((pos+7)&127)]=x;}else x=ram[((pos+7)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>6));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>6));}lastx=x;lastB=dB;
// slot 142

// slot 143
x=ram[((pos+6)&127)];mulb=C[143];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;A_e143=Aa;B_e143=Ba;
// slot 144
x=ram[((pos+69)&127)];mulb=C[144];Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;A_e144=Aa;B_e144=Ba;
// slot 145
x=ram[((pos+72)&127)];mulb=C[145];Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;A_e145=Aa;B_e145=Ba;
// slot 146
fl=C[146]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e143):sat24(A_e143);ram[((pos+69)&127)]=x;}else x=ram[((pos+69)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;A_e146=Aa;B_e146=Ba;
// slot 147
fl=C[147]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e144):sat24(A_e144);ram[((pos+0)&127)]=x;}else x=ram[((pos+0)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;B_e147=Ba;
// slot 148
fl=C[148]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e145):sat24(A_e145);ram[((pos+71)&127)]=x;}else x=ram[((pos+71)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;A_e148=Aa;B_e148=Ba;
// slot 149
fl=C[149]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e146):sat24(A_e146);ram[((pos+0)&127)]=x;}else x=ram[((pos+0)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;A_e149=Aa;B_e149=Ba;
// slot 150
x=sat24(B_e147);if(x<0)x=0;ram[((pos+68)&127)]=x;Ba=se30(Ba+((se24(x)*C[150])>>7));lastx=x;lastB=1;
// slot 151
fl=C[151]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e148):sat24(A_e148);ram[((pos+59)&127)]=x;}else x=ram[((pos+59)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;
// slot 152
fl=C[152]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e149):sat24(A_e149);ram[((pos+0)&127)]=x;}else x=ram[((pos+0)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;A_e152=Aa;
// slot 153
x=4194304;mulb=C[153];Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>6));lastx=x;lastB=0;
// slot 154
ram[((pos+126)&127)]=latch;x=latch;Ba=se30(Ba+((se24(x)*(C[154]))>>7));lastx=x;lastB=1;
// slot 155
x=sat24(A_e152);ram[((pos+70)&127)]=x;mulb=C[155];Aa=se30(Aa+((se24(x)*mulb)>>6));lastx=x;lastB=0;A_e155=Aa;
// slot 156
x=4194304;mulb=C[156];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;
// slot 157
x=4194304;mulb=C[157];Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>5));lastx=x;lastB=0;B_e157=Ba;
// slot 158
x=sat24(A_e155);ram[((pos+6)&127)]=x;mulb=C[158];Aa=se30(Aa+((se24(x)*mulb)>>6));lastx=x;lastB=0;
// slot 159
x=lastx>>7;if(lastB)Ba=se30(Ba+((se24(x)*(C[159]&0x7f))>>6));else Aa=se30(Aa+((se24(x)*(C[159]&0x7f))>>6));lastx=x;
// slot 160
t=sat24(B_e157);if(t!==reg[4])rs[4]=0;reg[4]=t;lastx=0;lastB=0;A_e160=Aa;B_e160=Ba;
// slot 161
x=4194304;mulb=C[161];Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>3));lastx=x;lastB=0;A_e161=Aa;B_e161=Ba;
// slot 162
t=sat24(A_e160);if(t!==reg[0])rs[0]=0;reg[0]=t;lastx=0;lastB=0;A_e162=Aa;B_e162=Ba;
if(hook)reg[0]=hook(162);
// slot 163
fl=C[163]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e160):sat24(A_e160);ram[((pos+126)&127)]=x;}else x=ram[((pos+126)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;A_e163=Aa;B_e163=Ba;
// slot 164
fl=C[164]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e161):sat24(A_e161);ram[((pos+0)&127)]=x;}else x=ram[((pos+0)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;B_e164=Ba;
// slot 165
fl=C[165]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e162):sat24(A_e162);ram[((pos+109)&127)]=x;}else x=ram[((pos+109)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;
// slot 166
fl=C[166]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e163):sat24(A_e163);ram[((pos+0)&127)]=x;}else x=ram[((pos+0)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;A_e166=Aa;B_e166=Ba;
// slot 167
x=sat24(B_e164);ram[((pos+6)&127)]=x;mulb=C[167];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>3));lastx=x;lastB=1;
// slot 168
x=lastx>>7;if(lastB)Ba=se30(Ba+((se24(x)*(C[168]&0x7f))>>3));else Aa=se30(Aa+((se24(x)*(C[168]&0x7f))>>3));lastx=x;A_e168=Aa;
// slot 169
fl=C[169]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e166):sat24(A_e166);ram[((pos+96)&127)]=x;}else x=ram[((pos+96)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;B_e169=Ba;
// slot 170
ea7=epos+16384;
x=ram[((pos+87)&127)];mulb=C[170];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;
// slot 171
armed=1;varOff=A_e168;t=(varOff&1023)<<13;if(t!==reg[0])rs[0]=0;reg[0]=t;lastx=0;lastB=0;
// slot 172
wl=sat24(B_e169);if(armed){t=0x7fffff-reg[0];if(t!==reg[3])rs[3]=0;reg[3]=t;}lastx=0;lastB=0;
// slot 173
eram[ea7&262143]=wl;
x=ram[((pos+88)&127)];mulb=C[173];Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;
// slot 174
armed=0;
x=ram[((pos+80)&127)];mulb=C[174];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;B_e174=Ba;
// slot 175
x=ram[((pos+81)&127)];mulb=C[175];Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;
// slot 176
t=sat24(B_e174);if(t!==reg[1])rs[1]=0;reg[1]=t;lastx=0;lastB=0;
// slot 177
x=ram[((pos+82)&127)];mulb=C[177];Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;A_e177=Aa;B_e177=Ba;
// slot 178

// slot 179
ea8=epos+(varOff>>10)+0;
x=ram[((pos+119)&127)];mulb=C[179];Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>6));lastx=x;lastB=0;
// slot 180
fl=C[180]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e177):sat24(A_e177);ram[((pos+6)&127)]=x;}else x=ram[((pos+6)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>6));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>6));}lastx=x;lastB=dB;B_e180=Ba;
// slot 181
x=1024;mulb=C[181];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;
// slot 182
latch=eram[ea8&262143];
x=lastx>>7;if(lastB)Ba=se30(Ba+((se24(x)*(C[182]&0x7f))>>7));else Aa=se30(Aa+((se24(x)*(C[182]&0x7f))>>7));lastx=x;
// slot 183
armed=0;
x=sat24(B_e180);ram[((pos+97)&127)]=x;mulb=C[183];Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;
// slot 184
ram[((pos+126)&127)]=latch;x=latch;Ba=se30(Ba+((se24(x)*(C[184]))>>7));lastx=x;lastB=1;
// slot 185
x=ram[((pos+103)&127)];mulb=C[185];Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;B_e185=Ba;
// slot 186
x=ram[((pos+103)&127)];mulb=C[186];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;
// slot 187
x=4194304;mulb=C[187];Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;B_e187=Ba;
// slot 188
ea9=epos+(varOff>>10)+1;
x=se24(B_e185);ram[((pos+102)&127)]=x;Ba=se30((se24(x)*C[188])>>7);lastx=x;lastB=1;
// slot 189
x=lastx>>7;if(lastB)Ba=se30(Ba+((se24(x)*(C[189]&0x7f))>>7));else Aa=se30(Aa+((se24(x)*(C[189]&0x7f))>>7));lastx=x;A_e189=Aa;
// slot 190
x=se24(B_e187);ram[((pos+6)&127)]=x;Ba=se30((se24(x)*C[190])>>7);lastx=x;lastB=1;A_e190=Aa;B_e190=Ba;
// slot 191
latch=eram[ea9&262143];
t=sat24(A_e189);if(t!==reg[1])rs[1]=0;reg[1]=t;lastx=0;lastB=0;A_e191=Aa;B_e191=Ba;
// slot 192
t=sat24(B_e190);if(t!==reg[4])rs[4]=0;reg[4]=t;lastx=0;lastB=0;
// slot 193
fl=C[193]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e190):sat24(A_e190);ram[((pos+127)&127)]=x;}else x=latch;mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;
// slot 194
fl=C[194]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e191):sat24(A_e191);ram[((pos+126)&127)]=x;}else x=ram[((pos+126)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;A_e194=Aa;
// slot 195
x=ram[((pos+116)&127)];mulb=C[195];Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));lastx=x;lastB=0;
// slot 196
x=ram[((pos+115)&127)];mulb=C[196];Aa=se30(Aa+((se24(x)*mulb)>>7));lastx=x;lastB=0;
// slot 197
x=sat24(A_e194);ram[((pos+114)&127)]=x;mulb=C[197];Aa=se30(Aa+((se24(x)*mulb)>>7));lastx=x;lastB=0;A_e197=Aa;
// slot 198
x=ram[((pos+116)&127)];mulb=C[198];Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));lastx=x;lastB=0;A_e198=Aa;B_e198=Ba;
// slot 199
x=ram[((pos+117)&127)];mulb=C[199];Aa=se30(Aa+((se24(x)*mulb)>>7));lastx=x;lastB=0;A_e199=Aa;B_e199=Ba;
// slot 200
x=sat24(A_e197);ram[((pos+115)&127)]=x;mulb=C[200];Aa=se30(Aa+((se24(x)*mulb)>>6));lastx=x;lastB=0;A_e200=Aa;B_e200=Ba;
// slot 201
fl=C[201]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e198):sat24(A_e198);ram[((pos+6)&127)]=x;}else x=ram[((pos+6)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;
// slot 202
fl=C[202]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e199):sat24(A_e199);ram[((pos+0)&127)]=x;}else x=ram[((pos+0)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;B_e202=Ba;
// slot 203
fl=C[203]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e200):sat24(A_e200);ram[((pos+116)&127)]=x;}else x=ram[((pos+116)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;
// slot 204
A_e204=Aa;
// slot 205
x=sat24(B_e202);ram[((pos+106)&127)]=x;mulb=C[205];Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;
// slot 206

// slot 207
x=sat24(A_e204);ram[((pos+31)&127)]=x;mulb=C[207];Aa=se30(Aa+((se24(x)*mulb)>>7));lastx=x;lastB=0;
oR=ram[((pos+31)&127)];
// slot 208
x=1024;mulb=C[208];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>5));lastx=x;lastB=1;
// slot 209
x=lastx>>7;if(lastB)Ba=se30(Ba+((se24(x)*(C[209]&0x7f))>>5));else Aa=se30(Aa+((se24(x)*(C[209]&0x7f))>>5));lastx=x;
// slot 210
x=ram[((pos+105)&127)];mulb=C[210];Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;B_e210=Ba;
// slot 211
x=ram[((pos+105)&127)];mulb=C[211];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;
// slot 212
x=4194304;mulb=C[212];Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;B_e212=Ba;
// slot 213
x=se24(B_e210);ram[((pos+104)&127)]=x;Ba=se30((se24(x)*C[213])>>7);lastx=x;lastB=1;
// slot 214
x=lastx>>7;if(lastB)Ba=se30(Ba+((se24(x)*(C[214]&0x7f))>>7));else Aa=se30(Aa+((se24(x)*(C[214]&0x7f))>>7));lastx=x;A_e214=Aa;B_e214=Ba;
// slot 215
x=se24(B_e212);ram[((pos+6)&127)]=x;Ba=se30((se24(x)*C[215])>>7);lastx=x;lastB=1;A_e215=Aa;B_e215=Ba;
// slot 216

// slot 217
t=sat24(B_e215);reg[0]=t<0?0:t;rs[0]=1;lastx=0;lastB=0;
// slot 218
fl=C[218]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e214):sat24(A_e214);ram[((pos+6)&127)]=x;}else x=ram[((pos+6)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;
// slot 219
fl=C[219]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e215):sat24(A_e215);ram[((pos+0)&127)]=x;}else x=ram[((pos+0)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;B_e219=Ba;
// slot 220
x=4194304;mulb=C[220];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;
// slot 221
x=ram[((pos+106)&127)];mulb=C[221];Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;
// slot 222
x=sat24(B_e219);ram[((pos+107)&127)]=x;mulb=C[222];Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;
// slot 223
x=65536;mulb=C[223];Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;B_e223=Ba;
// slot 224
x=ram[((pos+119)&127)];mulb=C[224];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;
// slot 225
x=65536;mulb=C[225];Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;A_e225=Aa;B_e225=Ba;
// slot 226
x=sat24(B_e223);ram[((pos+108)&127)]=x;mulb=C[226];Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;B_e226=Ba;
// slot 227
x=ram[((pos+61)&127)];mulb=C[227];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;
// slot 228
fl=C[228]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e225):sat24(A_e225);ram[((pos+68)&127)]=x;}else x=ram[((pos+68)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>6));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>6));}lastx=x;lastB=dB;
// slot 229
x=sat24(B_e226);if(x<0)x=0;ram[((pos+118)&127)]=x;Ba=se30(Ba+((se24(x)*C[229])>>7));lastx=x;lastB=1;
// slot 230
x=ram[((pos+68)&127)];mulb=C[230];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;
// slot 231

// slot 232

// slot 233
x=ram[((pos+61)&127)];mulb=C[233];Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;
// slot 234
B_e234=Ba;
// slot 235

// slot 236

// slot 237
x=sat24(B_e234);ram[((pos+58)&127)]=x;mulb=C[237];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;
// slot 238
x=ram[((pos+23)&127)];mulb=C[238];Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));lastx=x;lastB=0;
// slot 239
x=ram[((pos+27)&127)];mulb=C[239];Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>6));lastx=x;lastB=0;
// slot 240

// slot 241
lastx=0;lastB=0;
// slot 242
lastx=0;lastB=0;
// slot 243
x=4194304;mulb=C[243];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;
// slot 244
x=ram[((pos+90)&127)];mulb=C[244];Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;
// slot 245
x=ram[((pos+91)&127)];mulb=C[245];Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;B_e245=Ba;
// slot 246

// slot 247
x=ram[((pos+106)&127)];mulb=C[247];Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>5));lastx=x;lastB=0;
// slot 248
x=se24(B_e245);ram[((pos+89)&127)]=x;Ba=se30((se24(x)*C[248])>>6);lastx=x;lastB=1;A_e248=Aa;
// slot 249
x=ram[((pos+91)&127)];mulb=C[249];Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;
// slot 250
t=sat24(A_e248);if(t!==reg[4])rs[4]=0;reg[4]=t;lastx=0;lastB=0;B_e250=Ba;
// slot 251
x=ram[((pos+100)&127)];mulb=C[251];Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));lastx=x;lastB=0;
// slot 252
x=ram[((pos+101)&127)];mulb=C[252];Aa=se30(Aa+((se24(x)*mulb)>>7));lastx=x;lastB=0;
// slot 253
x=se24(B_e250);ram[((pos+90)&127)]=x;Ba=se30((se24(x)*C[253])>>6);lastx=x;lastB=1;
// slot 254
x=ram[((pos+94)&127)];mulb=C[254];Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;A_e254=Aa;B_e254=Ba;
// slot 255
x=ram[((pos+97)&127)];mulb=C[255];Aa=se30(Aa+((se24(x)*mulb)>>6));lastx=x;lastB=0;
// slot 256
x=ram[((pos+94)&127)];mulb=C[256];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;A_e256=Aa;
// slot 257
fl=C[257]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e254):sat24(A_e254);ram[((pos+92)&127)]=x;}else x=ram[((pos+92)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;
// slot 258
x=ram[((pos+92)&127)];mulb=C[258];Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;B_e258=Ba;
// slot 259
x=sat24(A_e256);ram[((pos+98)&127)]=x;mulb=C[259];Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));lastx=x;lastB=0;A_e259=Aa;B_e259=Ba;
// slot 260
x=ram[((pos+100)&127)];mulb=C[260];Aa=se30(Aa+((se24(x)*mulb)>>6));lastx=x;lastB=0;
// slot 261
x=sat24(B_e258);ram[((pos+93)&127)]=x;mulb=C[261];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;A_e261=Aa;
// slot 262
fl=C[262]&255;dB=(fl&2)?0:1;rep=!(fl&1);r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{if(fl&4){x=dB?sat24(B_e259):sat24(A_e259);ram[((pos+92)&127)]=x;}else x=ram[((pos+92)&127)];mulb=((r>>16)<<24)>>24;}if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));}lastx=x;lastB=dB;B_e262=Ba;
// slot 263
x=ram[((pos+101)&127)];mulb=C[263];Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>6));lastx=x;lastB=0;
// slot 264
x=sat24(A_e261);ram[((pos+99)&127)]=x;mulb=C[264];Aa=se30(Aa+((se24(x)*mulb)>>7));lastx=x;lastB=0;A_e264=Aa;
// slot 265
x=sat24(B_e262);ram[((pos+6)&127)]=x;mulb=C[265];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;B_e265=Ba;
// slot 266
x=ram[((pos+99)&127)];mulb=C[266];Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));lastx=x;lastB=0;
// slot 267
x=sat24(A_e264);ram[((pos+100)&127)]=x;mulb=C[267];Aa=se30(Aa+((se24(x)*mulb)>>7));lastx=x;lastB=0;
// slot 268
x=sat24(B_e265);ram[((pos+6)&127)]=x;mulb=C[268];Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;A_e268=Aa;
// slot 269
x=4194304;mulb=C[269];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>6));lastx=x;lastB=1;B_e269=Ba;
// slot 270
x=ram[((pos+6)&127)];mulb=C[270];Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>7));lastx=x;lastB=0;
// slot 271
x=sat24(A_e268);ram[((pos+7)&127)]=x;mulb=C[271];Aa=se30(Aa+((se24(x)*mulb)>>5));lastx=x;lastB=0;
// slot 272
x=sat24(B_e269);ram[((pos+120)&127)]=x;mulb=C[272];Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;
// slot 273
x=lastx>>7;if(lastB)Ba=se30(Ba+((se24(x)*(C[273]&0x7f))>>7));else Aa=se30(Aa+((se24(x)*(C[273]&0x7f))>>7));lastx=x;A_e273=Aa;
// slot 274
x=ram[((pos+121)&127)];mulb=C[274];Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;
// slot 275
x=lastx>>7;if(lastB)Ba=se30(Ba+((se24(x)*(C[275]&0x7f))>>7));else Aa=se30(Aa+((se24(x)*(C[275]&0x7f))>>7));lastx=x;B_e275=Ba;
// slot 276
x=sat24(A_e273);ram[((pos+95)&127)]=x;mulb=C[276];Aa=se30(Aa+((se24(x)*mulb)>>7));lastx=x;lastB=0;
// slot 277

// slot 278
x=sat24(B_e275);ram[((pos+120)&127)]=x;mulb=C[278];Ba=se30(Ba+((se24(x)*mulb)>>7));lastx=x;lastB=1;
// slot 279

// slot 280

// slot 281

// slot 282
A_e282=Aa;B_e282=Ba;
// slot 283
A_e283=Aa;B_e283=Ba;
// slot 284
A_e284=Aa;B_e284=Ba;
// slot 285
A_e285=Aa;B_e285=Ba;
A_m0n=A_e282;B_m0n=B_e282;A_q0n=A_e282;B_q0n=B_e282;A_m1n=A_e283;B_m1n=B_e283;A_q1n=A_e283;B_q1n=B_e283;A_m2n=A_e284;B_m2n=B_e284;A_q2n=A_e284;B_q2n=B_e284;A_m3n=A_e285;B_m3n=B_e285;A_q3n=A_e285;B_q3n=B_e285;
A_m0=A_m0n;B_m0=B_m0n;A_q0=A_q0n;B_q0=B_q0n;A_m1=A_m1n;B_m1=B_m1n;A_q1=A_q1n;B_q1=B_q1n;A_m2=A_m2n;B_m2=B_m2n;A_q2=A_q2n;B_q2=B_q2n;A_m3=A_m3n;B_m3=B_m3n;A_q3=A_q3n;B_q3=B_q3n;
 outL[n]=oL; outR[n]=oR;
 pos=(pos-1)&127; epos=epos-1;
}
st.H[0]=A_m0;st.H[1]=B_m0;st.H[2]=A_m1;st.H[3]=B_m1;st.H[4]=A_m2;st.H[5]=B_m2;st.H[6]=A_m3;st.H[7]=B_m3;st.H[8]=A_q0;st.H[9]=B_q0;st.H[10]=A_q1;st.H[11]=B_q1;st.H[12]=A_q2;st.H[13]=B_q2;st.H[14]=A_q3;st.H[15]=B_q3;
st.pos=pos; st.epos=epos; st.Aa=Aa; st.Ba=Ba; st.latch=latch; st.wl=wl; st.varOff=varOff; st.armed=armed; st.lastx=lastx; st.lastB=lastB;
};})(typeof globalThis!=="undefined"?globalThis:this);
// MR2 (Roland TC203C040AF) interpreter, specialised to the decoded SP-303 Vinyl Sim program.
// Mirrors sim/mr2v7.c with cfg_v8 (erp=2 ERAM protocol, latch MACs, a6 write latch, ...).
// The program structure is compiled to straight-line JS once; coefficients stay live.
(function (root) {
  'use strict';
  const SH = [7, 6, 5, 3];
  const ERAM_MASK = (1 << 18) - 1;

  function compile(words) {
    const N = words.length;
    // Static ERAM schedule: cmd word + 5 chunk words, transfer 8 slots after the command.
    const startAt = {}, addrAt = {}, xferAt = {};
    let busy = false, pk = 0, cur = null, just = false;
    const txs = [];
    for (let i = 0; i < N; i++) {
      const f = words[i] >>> 21;
      if (busy) {
        pk++;
        if (pk <= 5) { cur.imm |= f << (1 + 3 * (pk - 1)); if (f & 1) cur.maskc++; if (f & 2) cur.bit1 = 1; }
        if (pk === 5) { addrAt[i] = cur; busy = false; just = true; }
      }
      if (!busy && !just && (f & 3)) {
        cur = { start: i, cmd: f & 3, imm: (f >> 2) & 1, maskc: 0, bit1: 0, id: txs.length };
        txs.push(cur); startAt[i] = cur; busy = true; pk = 0;
      }
      just = false;
    }
    for (const t of txs) {
      t.write = t.cmd === 2 || (t.cmd === 3 && t.bit1);
      const x = t.start + 8;
      if (x >= N || t.start + 5 >= N) throw new Error('ERAM transaction wraps the sample boundary (slot ' + t.start + ')');
      (xferAt[x] = xferAt[x] || []).push(t);
    }
    // Accumulator history is resolved at compile time: every pipeline read names the
    // exact earlier result, so no ring buffers run per slot. Main (store/ERAM) pipeline M
    // and register-load pipeline R advance together except at sp.a9 (holds M) and
    // sp.a5/b5 (hold R). The last four entries of each persist across samples.
    const pass = (need) => {
    const L = [];
    const used = new Set();
    let M = ['m0', 'm1', 'm2', 'm3'], R = ['q0', 'q1', 'q2', 'q3'], cnt = 0;
    const ref = (h, d, p) => { const id = h[h.length - d]; used.add(p + id); return p + '_' + id; };
    const pipeB = (d) => `sat24(${ref(M, d, 'B')})`, pipeA = (d) => `sat24(${ref(M, d, 'A')})`;
    const rawB = (d) => ref(M, d, 'B'), rawA = (d) => ref(M, d, 'A');
    const regB = () => `sat24(${ref(R, 2, 'B')})`, regA = () => `sat24(${ref(R, 2, 'A')})`;
    const save = (id) => (need.has('A' + id) ? `A_${id}=Aa;` : '') + (need.has('B' + id) ? `B_${id}=Ba;` : '');
    const adv = (kind) => { const id = 'e' + (cnt++);
      if (kind === 'a9') M[M.length - 1] = id; else M.push(id);
      if (kind === 'lo') R[R.length - 1] = id; else R.push(id);
      return save(id); };
    const RAMI = (a) => `((pos+${a})&127)`;
    const RD = (a) => (a >= 1 && a <= 4) ? String(0x10 << (6 * (a - 1))) : (a === 0x7f ? 'latch' : `ram[${RAMI(a)}]`);
    const setReg = (k, v) => `t=${v};if(t!==reg[${k}])rs[${k}]=0;reg[${k}]=t;`;
    for (let i = 0; i < N; i++) {
      const w = words[i];
      L.push(`// slot ${i}`);
      // ERAM controller (before the instruction, as in the C model)
      if (xferAt[i]) for (const t of xferAt[i]) {
        L.push(t.write ? `eram[ea${t.id}&${ERAM_MASK}]=wl;` : `latch=eram[ea${t.id}&${ERAM_MASK}];`);
      }
      if (addrAt[i]) {
        const t = addrAt[i];
        if (t.cmd === 3) L.push(`ea${t.id}=${t.maskc === 5 ? '0' : 'epos'}+(varOff>>10)+${t.imm & 1};`);
        else L.push(`ea${t.id}=epos+${t.imm << 3};`);
      }
      if (startAt[i] && startAt[i].cmd === 3) L.push('armed=0;');
      if (!w) { L.push(adv('')); continue; }
      const op = (w >> 18) & 7, m = (w >> 10) & 0xff, sh = SH[(w >> 8) & 3], b7 = m >> 7, am = m & 0x7f;
      const C = `C[${i}]`;
      let body = '';
      if (op <= 3) {
        const dstB = op <= 1;
        const acc = dstB ? 'Ba' : 'Aa';
        const store = (op === 1 || op === 3) && !(am >= 1 && am <= 4);
        if (store) body += `x=${dstB ? pipeB(3) : pipeA(3)};ram[${RAMI(am)}]=x;`;
        else body += `x=${RD(am)};`;
        body += `mulb=${C};${b7 ? `${acc}=0;` : ''}${acc}=se30(${acc}+((se24(x)*mulb)>>${sh}));lastx=x;lastB=${dstB ? 1 : 0};`;
      } else if (op === 4) {
        body += `x=se24(${rawB(3)});ram[${RAMI(am)}]=x;`;
        if (b7) body += `if(x<0)x=(~x)&0x7fffff;`;
        body += `Ba=se30((se24(x)*${C})>>${sh});lastx=x;lastB=1;`;
      } else if (op === 6) {
        body += `x=${pipeB(3)};if(x<0)x=0;ram[${RAMI(am)}]=x;Ba=se30(Ba+((se24(x)*${C})>>${sh}));lastx=x;lastB=1;`;
      } else if (op === 5 && !b7) {
        // op5 with operand bit 7 clear: low-order coefficient continuation of the preceding MAC
        // (like sp.a5/b5): x = lastx>>7, 7-bit coefficient, the head's precision (>>7), same destination.
        body += `x=lastx>>7;if(lastB)Ba=se30(Ba+((se24(x)*(${C}&0x7f))>>7));else Aa=se30(Aa+((se24(x)*(${C}&0x7f))>>7));lastx=x;`;
      } else if (op === 5) {
        // runtime flags (the flag byte is the live coefficient byte)
        body += `fl=${C}&255;dB=(fl&2)?0:1;rep=!(fl&1);`;
        body += `r=((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?reg[3]:reg[((fl>>7)&1)*4+((fl>>5)&1)];`;
        body += `if(fl&0x10){if(rs[((fl&0x10)&&((((fl>>7)&1)*4+((fl>>5)&1))===1))?3:((fl>>7)&1)*4+((fl>>5)&1)])r=-r;else r=r>=0?((~r)&0x7fffff):~(r&0x7fffff);}`;
        body += `if(fl&0x40){x=lastx>>7;mulb=(r>>9)&0x7f;}else{`;
        body += `if(fl&4){x=dB?${pipeB(3)}:${pipeA(3)};ram[${RAMI(am)}]=x;}else x=${RD(am)};`;
        body += `mulb=((r>>16)<<24)>>24;}`;
        body += `if(dB){if(rep)Ba=0;Ba=se30(Ba+((se24(x)*mulb)>>${sh}));}else{if(rep)Aa=0;Aa=se30(Aa+((se24(x)*mulb)>>${sh}));}lastx=x;lastB=dB;`;
      } else { // op 7 specials
        const mac = (accB, xs, cs) => `x=${xs};${accB ? 'Ba' : 'Aa'}=se30(${accB ? 'Ba' : 'Aa'}+((se24(x)*(${cs}))>>${sh}));lastx=x;lastB=${accB ? 1 : 0};`;
        const none = 'lastx=0;lastB=0;';
        switch (m) {
          case 0xe7: body += `armed=1;varOff=${rawB(3)};` + setReg(0, '(varOff&1023)<<13') + none; break;
          case 0xf7: body += `armed=1;varOff=${rawA(3)};` + setReg(0, '(varOff&1023)<<13') + none; break;
          case 0xee: case 0xef: case 0xcf: case 0xff: case 0xdf: case 0xce: case 0xfe: case 0xde:
            body += `ram[${RAMI(0x70 | (m & 0xf))}]=latch;` + mac(!(m & 0x10), 'latch', C); break;
          case 0xb6: body += `wl=${pipeA(3)};` + none; break;
          case 0xa6: body += `wl=${pipeB(3)};if(armed){` + setReg(3, '0x7fffff-reg[0]') + `}` + none; break;
          // sp.a9: signed load (sets the register's signed flag), clamps negatives to 0, does not advance the A/B pipeline
          case 0xa9: body += `t=${regB()};reg[0]=t<0?0:t;rs[0]=1;` + none; break;
          case 0xaa: body += setReg(4, regB()) + none; break;
          case 0xe5: body += setReg(1, regB()) + none; break;
          case 0xb9: body += setReg(0, regA()) + none; break;
          case 0xba: body += setReg(4, regA()) + none; break;
          case 0xf5: body += setReg(1, regA()) + none; break;
          case 0xa5: case 0xb5:
            body += `x=lastx>>7;if(lastB)Ba=se30(Ba+((se24(x)*(${C}&0x7f))>>${sh}));else Aa=se30(Aa+((se24(x)*(${C}&0x7f))>>${sh}));lastx=x;`; break;
          default: body += none;
        }
      }
      const isA9 = op === 7 && m === 0xa9, isLo = (op === 7 && (m === 0xa5 || m === 0xb5)) || (op === 5 && !b7);
      L.push(body + adv(isA9 ? 'a9' : isLo ? 'lo' : ''));
      if (i === 54 || i === 162) L.push(`if(hook)reg[0]=hook(${i});`);
      if (i === OUT.L) L.push(`oL=ram[${RAMI((words[i] >> 10) & 0x7f)}];`);
      if (i === OUT.R) L.push(`oR=ram[${RAMI((words[i] >> 10) & 0x7f)}];`);
    }
    // persist the last four entries of each pipeline for the next sample
    const tail = [];
    for (let k = 0; k < 4; k++) { const a = M[M.length - 4 + k], b = R[R.length - 4 + k];
      used.add('A' + a); used.add('B' + a); used.add('A' + b); used.add('B' + b);
      tail.push([`m${k}`, a], [`q${k}`, b]); }
    L.push(tail.map(([p, id]) => `A_${p}n=A_${id};B_${p}n=B_${id};`).join(''));
    L.push(tail.map(([p]) => `A_${p}=A_${p}n;B_${p}=B_${p}n;`).join(''));
    return { L, used };
    };
    const first = pass(new Set());
    const { L, used } = pass(first.used);
    const vars = [...used].filter(v => !/_?[mq]\d$/.test(v.slice(1))).map(v => `${v[0]}_${v.slice(1)}=0`);
    const pers = ['m0','m1','m2','m3','q0','q1','q2','q3'];
    const decl = 'let x=0,mulb=0,fl=0,r=0,rep=0,dB=0,t=0' + (vars.length ? ',' + vars.join(',') : '') + ',' + pers.map(p => `A_${p}n=0,B_${p}n=0`).join(',') + ';\n' +
      pers.map((p, k) => `let A_${p}=st.H[${2 * k}],B_${p}=st.H[${2 * k + 1}];`).join('') + '\n';
    const fin = pers.map((p, k) => `st.H[${2 * k}]=A_${p};st.H[${2 * k + 1}]=B_${p};`).join('');
    const eaDecl = txs.map(t => `ea${t.id}`).join(',');
    return { src: L.join('\n'), eaDecl, txs, decl, fin };
  }
  let OUT = { L: 102, R: 207 };

  class VinylMR2 {
    constructor(data) {
      this.data = data; OUT = { L: data.outL, R: data.outR };
      this.words = data.words.slice();
      this.C = new Int32Array(this.words.length);
      for (let i = 0; i < this.words.length; i++) this.C[i] = ((this.words[i] & 0xff) << 24) >> 24;
      const pre = (typeof root.mr2kernel === 'function') ? root.mr2kernel : null;
      const { src, eaDecl, decl, fin } = pre ? { src: '', eaDecl: '', decl: '', fin: '' } : compile(this.words);
      const fn = `"use strict";
const sat24=(v)=>v>0x7fffff?0x7fffff:(v<-0x800000?-0x800000:v);
const se24=(v)=>(v<<8)>>8; const se30=(v)=>(v<<2)>>2;
const ram=st.ram, reg=st.reg, rs=st.rs, eram=st.eram, C=st.C;
let pos=st.pos, epos=st.epos, Aa=st.Aa, Ba=st.Ba, latch=st.latch, wl=st.wl, varOff=st.varOff, armed=st.armed, lastx=st.lastx, lastB=st.lastB;
let ${eaDecl || 'ea_'};
const hook=st.hook;
${decl}
for(let n=0;n<len;n++){
 ram[(pos+0x38)&127]=inL[n]; ram[(pos+0x39)&127]=inR[n];
 if(st.pre)st.pre(inL[n],inR[n]);
 let oL=0,oR=0;
 ${src}
 outL[n]=oL; outR[n]=oR;
 pos=(pos-1)&127; epos=epos-1;
}
${fin}
st.pos=pos; st.epos=epos; st.Aa=Aa; st.Ba=Ba; st.latch=latch; st.wl=wl; st.varOff=varOff; st.armed=armed; st.lastx=lastx; st.lastB=lastB;`;
      this.kernel = pre || new Function('st', 'inL', 'inR', 'outL', 'outR', 'len', fn);
      this.reset();
    }
    reset() {
      this.st = { ram: new Int32Array(128), reg: new Int32Array(8), rs: new Int8Array(8), eram: new Int32Array(1 << 18), H: new Int32Array(16),
        C: this.C, pos: 0, epos: 0, Aa: 0, Ba: 0, latch: 0, wl: 0, varOff: 0, armed: 0, lastx: 0, lastB: 1, hook: null, pre: null };
    }
    patch8(addr, v) { this.C[addr - 223] = ((v & 0xff) << 24) >> 24; }
    patch16(addr, v) { const i = addr - 223; this.C[i] = (((v >> 8) & 0xff) << 24) >> 24; this.C[i + 1] = ((v & 0xff) >> 1); }
    setKnobs(comp, noise, wow) {
      const T = this.data.tables;
      for (const [a, t] of this.data.p16) this.patch16(a, T[t][comp]);
      for (const [a, t] of this.data.p8) this.patch8(a, T[t][t === 'W492' ? wow : noise]);
      for (const [a, v] of this.data.fixed8) this.patch8(a, v);
    }
    process(inL, inR, outL, outR, len) { this.kernel(this.st, inL, inR, outL, outR, len); }
    // Compressor model. 'literal' = the firmware program itself (default, bit-exact with sim/mr2v7.c rules np=2 clr=1 sgn=1 lo5=1, op5b0=6).
    // 'recon' = reconstructed feedback gain computer, built from the decoded detector chain
    // (threshold, square x3.5, multiplicative attack, linear release, L+R link, 1/8 smoothing,
    // gain = 1 - CA*(1 - s)); it drives the gain register the program loads at slots 54/162.
    setCompMode(mode, p) {
      this.mode = mode;
      if (mode !== 'recon') {
        // Firmware mode: the program runs untouched; the hook only reads the gain register
        // the program itself loads at slot 54 (sp.a9) to drive the gain-reduction meter.
        const R = this.rc = this.rc || { minG: 1 }; const reg = this.st.reg;
        this.st.pre = null;
        this.st.hook = function (slot) { const v = reg[0]; if (slot === 54) { const g = v / 8388608; if (g < R.minG) R.minG = g; } return v; };
        return;
      }
      const P = Object.assign({ thr: 0.0098, k: 3.5, rel: 3.05e-5, link: 0.125 }, p || {});
      const R = this.rc = Object.assign({ eL: 1, eR: 1, sL: 1, sR: 1, G: 1, minG: 1 }, this.rc && 'eL' in this.rc ? this.rc : {});
      R.P = P;
      const self = this;
      this.st.pre = function (l, r) {
        const P = R.P, CA = self.CA;
        let xl = 2 * l / 8388608, xr = 2 * r / 8388608;
        xl = xl > 1 ? 1 : xl < -1 ? -1 : xl; xr = xr > 1 ? 1 : xr < -1 ? -1 : xr;
        let ul = Math.abs(xl * R.eL) - P.thr, ur = Math.abs(xr * R.eR) - P.thr;
        const gl = ul > 0 ? Math.min(1, P.k * ul * ul) : 0, gr = ur > 0 ? Math.min(1, P.k * ur * ur) : 0;
        R.eL = Math.min(1, Math.max(0, R.eL * (1 - gl) + P.rel));
        R.eR = Math.min(1, Math.max(0, R.eR * (1 - gr) + P.rel));
        const lk = Math.min(1, R.eL + R.eR);
        R.sL += (lk - R.sL) * P.link; R.sR += (lk - R.sR) * P.link;
        const GL = Math.max(-1, Math.min(1, 1 + CA * (R.sL - 1))), GR = Math.max(-1, Math.min(1, 1 + CA * (R.sR - 1)));
        R.GL = GL; R.GR = GR; if (GL < R.minG) R.minG = GL;
      };
      this.st.hook = function (slot) {
        const g = slot === 54 ? R.GL : R.GR;
        return g >= 1 ? 0x7fffff : Math.round(g * 8388608);
      };
    }
    setKnobsAll(comp, noise, wow) {
      this.setKnobs(comp, noise, wow);
      this.CA = this.data.tables.CA[comp] / 16384;
    }
    takeMinGain() { const r = this.rc; if (!r) return 1; const g = r.minG; r.minG = 1; return g; }
  }
  root.VinylMR2 = VinylMR2;
  if (typeof module !== 'undefined') module.exports = { VinylMR2 };
})(typeof globalThis !== 'undefined' ? globalThis : this);


// Editable source for lavalite-architecture.pdf
// Regenerate: node lavalite-architecture.js [output-directory]
const pptxgen = require('pptxgenjs');
const path = require('path');
const out = process.argv[2] || __dirname;
const pptx = new pptxgen();
pptx.layout = 'LAYOUT_WIDE';
pptx.author = 'LavaLite Contributors';
pptx.subject = 'LavaLite batch scheduler architecture';
pptx.title = 'LavaLite - Batch Scheduler Architecture';
pptx.lang = 'en-US';
pptx.theme = {headFontFace:'Aptos Display',bodyFontFace:'Aptos',lang:'en-US'};
const C = {bg:'101B2B', panel:'1F3045', deep:'16263A', white:'F1F5FB', muted:'C5D0E0', cyan:'53C7EC', green:'65D8A2', gold:'F0BF65', red:'E99B90', border:'4D6581'};
function txt(s,t,x,y,w,h,sz=17,color=C.white,opts={}) {
    s.addText(t,{x,y,w,h,fontFace:'Aptos',fontSize:sz,color,margin:0,valign:'mid',breakLine:false,...opts});
}
function box(s,x,y,w,h,fill=C.panel,stroke=null) {
    s.addShape('roundRect',{x,y,w,h,rectRadius:.1,fill:{color:fill},line:stroke?{color:stroke,width:1.3}:{color:fill,transparency:100},radius:.1});
}
function line(s,x1,y1,x2,y2,color=C.cyan,width=2,arrow=false){
    s.addShape('line',{x:x1,y:y1,w:x2-x1,h:y2-y1,line:{color,width,...(arrow?{endArrowType:'triangle'}:{})}});
}
function base(title,subtitle,n) {
    let s=pptx.addSlide(); s.background={color:C.bg};
    txt(s,title,.55,.34,12.15,.53,28,C.white,{bold:true});
    txt(s,subtitle,.56,1.02,12,.52,16,C.muted);
    line(s,.53,7.08,12.81,7.08,C.border,1);
    txt(s,'LavaLite  |  Batch scheduler architecture',.55,7.14,8,.18,10,C.gold);
    txt(s,String(n),12.35,7.13,.35,.22,10,C.muted,{align:'right'});
    return s;
}
function card(s,x,y,w,h,title,body,accent=C.cyan,sz=17) {
    box(s,x,y,w,h);
    txt(s,title,x+.23,y+.19,w-.46,.46,20,accent,{bold:true});
    txt(s,Array.isArray(body)?body.join('\n'):body,x+.23,y+.77,w-.46,h-.96,sz,C.white,{valign:'top',breakLine:false,lineSpacingMultiple:1.08});
}
function tag(s,x,y,w,label,color=C.cyan){box(s,x,y,w,.46,C.deep,color);txt(s,label,x+.14,y+.08,w-.28,.27,13,C.white,{bold:true});}
let s;
// 1. scope and architecture
s=base('One scheduler, two daemons','Native batch execution on Linux compute hosts; one management daemon and an execution daemon per host.',1);
card(s,.55,1.75,3.8,3.54,'Submission',['bsub submits jobs','bjobs, bhist query state','bkill, bmove, bpriority','C client API: llbatch'],C.cyan,18);
card(s,4.76,1.75,3.8,3.54,'Control plane',['mbd: Master Batch Daemon','Queue and host policy','Scheduling and state','Durable event manifest'],C.gold,18);
card(s,8.97,1.75,3.8,3.54,'Execution plane',['sbd: Slave Batch Daemon','One on each execution host','Launch and control jobs','cgroup v2 resource limits'],C.green,18);
txt(s,'Linux-only. No external scheduler database and no second control plane.',.6,5.82,12.1,.65,21,C.cyan,{bold:true});
// 2. topological path
s=base('Job submission and dispatch','The client talks to mbd; execution daemons connect to mbd and receive work over those connections.',2);
box(s,.53,1.92,2.66,2.48);txt(s,'User / CLI',.74,2.19,2.2,.42,21,C.cyan,{bold:true});txt(s,'bsub\nbjobs / bhist\nbkill',.75,2.85,2.16,1.27,18);
box(s,4.14,1.92,3.5,2.48);txt(s,'mbd',4.43,2.18,2.95,.46,25,C.gold,{bold:true});txt(s,'Validate and queue\nSelect host and dispatch\nRecord state transitions',4.43,2.85,2.95,1.27,17);
box(s,8.6,1.77,4.13,1.1);txt(s,'sbd - host A',8.84,1.96,3.6,.4,20,C.green,{bold:true});
box(s,8.6,3.33,4.13,1.1);txt(s,'sbd - host B',8.84,3.52,3.6,.4,20,C.green,{bold:true});
line(s,3.2,3.05,4.06,3.05,C.cyan,2.4,true);line(s,7.68,2.78,8.51,2.3,C.green,2.4,true);line(s,7.68,3.4,8.51,3.89,C.green,2.4,true);
box(s,.57,5.12,12.15,1.04,C.deep);txt(s,'Transport: TCP  |  Encoding: XDR  |  Request authentication: HMAC-SHA256',.89,5.35,11.4,.58,18,C.white,{bold:true});
// 3. scheduling model
s=base('How mbd chooses where a job runs','Scheduling uses configured policy and available resources; pending jobs remain under scheduler control.',3);
card(s,.55,1.75,3.82,3.78,'Policy',['Queues and priorities','Eligible host groups','User and queue limits','Hold, begin and deadline times'],C.cyan,17.5);
card(s,4.75,1.75,3.82,3.78,'Resources',['CPU, memory and storage','GPU requirements','Shared token pools','Exclusive host constraints'],C.gold,17.5);
card(s,8.95,1.75,3.82,3.78,'Jobs',['PENDING until eligible','Dependencies and job arrays','RUNNING after dispatch','History available via bhist'],C.green,17.5);
txt(s,'mbd selects the execution host; sbd launches and supervises the job.',.61,5.93,11.95,.54,21,C.cyan,{bold:true});
// 4. execution
s=base('Execution on a worker','sbd runs the job under the submitting user, tracks its processes, and enforces allocated resources.',4);
box(s,.55,1.76,5.56,4.29,C.panel);txt(s,'Execution host',.83,1.99,4.95,.44,22,C.green,{bold:true});
tag(s,.83,2.72,4.95,'sbd (runs as root)',C.green);
line(s,3.29,3.18,3.29,3.65,C.green,2,true);
tag(s,.83,3.72,4.95,'Dedicated job cgroup (cgroup v2)',C.cyan);
line(s,3.29,4.18,3.29,4.66,C.cyan,2,true);
tag(s,.83,4.74,4.95,'Job script + child processes',C.gold);
card(s,6.49,1.76,6.27,4.29,'Enforcement and control',['CPU and memory limits use cgroup v2','Signals reach processes in the job cgroup','STOP / CONT / TERM / KILL are requested operations','sbd reports state changes back to mbd'],C.cyan,17);
// 5 job lifetime
s=base('Job lifetime is defined by the cgroup','The job script exit does not, by itself, mean the job has finished.',5);
let steps=[['1','Launch','sbd starts the job script and tracks its cgroup'],['2','Script exits','Its exit status is retained as the script status'],['3','Children remain','The job stays RUNNING while the cgroup is populated'],['4','Last process exits','sbd can finish the job and report completion']];
steps.forEach((a,i)=>{const y=1.76+i*1.02;box(s,.58,y,12.15,.84,i%2?C.deep:C.panel);box(s,.8,y+.18,.5,.47,C.cyan);txt(s,a[0],.96,y+.27,.2,.23,14,C.bg,{bold:true});txt(s,a[1],1.58,y+.15,2.3,.5,19,C.gold,{bold:true});txt(s,a[2],4.0,y+.14,8.38,.52,17);});
box(s,.6,6.10,12.12,.57,C.deep);txt(s,'sbd does not autonomously kill remaining processes merely because the script exits.',.84,6.25,11.55,.3,17,C.green,{bold:true});
// 6 durability
s=base('State survives daemon restart','The master reconstructs scheduler state from a durable event manifest; sbd also maintains execution state.',6);
card(s,.55,1.78,5.92,3.96,'mbd recovery',['Apply valid state transitions','Record durable events','Replay the manifest on restart','Reconstruct queues, jobs and dispatch state'],C.gold,17.5);
card(s,6.84,1.78,5.92,3.96,'sbd recovery',['Maintain execution state on disk','Recover running jobs after restart','Reconcile process state and report to mbd','Preserve active jobs through daemon restart'],C.green,17.5);
txt(s,'Recovery is part of the scheduler design, not an external database service.',.58,6.06,12,.58,20,C.cyan,{bold:true});
// 7 interface + boundaries
s=base('A small, explicit architecture','One C API, text configuration, and Linux facilities that administrators can inspect.',7);
card(s,.55,1.77,3.82,3.8,'Interfaces',['llbatch C API','CLI clients built on the API','bsub / bjobs / bhist','bqueues / bhosts / bgroups'],C.cyan,17.5);
card(s,4.75,1.77,3.82,3.8,'Configuration',['ll.conf: environment','llb.queues: policies','llb.hosts: hosts, resources','Text files; no external DB'],C.gold,17.5);
card(s,8.95,1.77,3.82,3.8,'Boundaries',['GPU placement is scheduler-enforced','No OS-level GPU device isolation yet','Multi-host jobs launch on first host','Services are covered separately'],C.green,17);
txt(s,'Companion document: lavalite-service-architecture.pdf',.6,6.03,11.9,.6,20,C.cyan,{bold:true});
pptx.writeFile({fileName:path.join(out,'lavalite-architecture.pptx')});

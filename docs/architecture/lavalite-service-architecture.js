const pptxgen = require('pptxgenjs');
const path = require('path');
const outputDir = process.argv[2] || __dirname;
const pptx = new pptxgen();
pptx.layout='LAYOUT_WIDE'; pptx.author='LavaLite Contributors'; pptx.subject='LavaLite architecture'; pptx.title='LavaLite Architecture: Batch Jobs and Persistent Services'; pptx.lang='en-US';
pptx.theme={headFontFace:'Aptos Display',bodyFontFace:'Aptos',lang:'en-US'};
const C={bg:'101B2B',panel:'1F3045',panel2:'17263A',white:'F0F5FC',muted:'BECADD',cyan:'53C7EC',green:'65D8A2',gold:'F0BF65',line:'5C7690',red:'E99B90'};
const W=13.333,H=7.5;
function tx(s,t,x,y,w,h,sz=17,color=C.white,opt={}){s.addText(t,{x,y,w,h,fontFace:'Aptos',fontSize:sz,color,margin:0,breakLine:false,valign:'mid',...opt});}
function rect(s,x,y,w,h,fill=C.panel,line=null,r=0.08){s.addShape(r?'roundRect':'rect',{x,y,w,h,rectRadius:r,fill:{color:fill},line:line?{color:line,width:1.2}:{color:fill,transparency:100},radius:r});}
function base(title,sub='',num){let s=pptx.addSlide();s.background={color:C.bg};tx(s,title,.52,.36,12.2,.5,28,C.white,{bold:true});if(sub) tx(s,sub,.54,1.02,12.1,.44,16,C.muted);s.addShape('line',{x:.53,y:7.1,w:12.26,h:0,line:{color:'34485E',width:1}});tx(s,'LavaLite  |  Architecture',.54,7.16,5,.18,10,C.gold);tx(s,String(num),12.3,7.13,.4,.24,10,C.muted,{align:'right'});return s;}
function block(s,x,y,w,h,title,lines,accent=C.cyan){rect(s,x,y,w,h);tx(s,title,x+.21,y+.17,w-.42,.42,20,accent,{bold:true});let t=Array.isArray(lines)?lines.join('\n'):lines;tx(s,t,x+.22,y+.72,w-.44,h-.88,16,C.white,{valign:'top',breakLine:false,lineSpacingMultiple:1.08});}
function arrow(s,x1,y1,x2,y2,color=C.cyan){s.addShape('line',{x:x1,y:y1,w:x2-x1,h:y2-y1,line:{color,width:2,beginArrowType:'none',endArrowType:'triangle'}});}
let s;
s=base('One scheduler for batch jobs and services','A small Linux scheduler with shared scheduling, resource control, and state recovery.',1);
block(s,.53,1.72,3.9,3.75,'Scheduling',['Queues, priorities and host selection','CPU, memory, storage and GPUs','Job arrays and dependencies','Tokens and exclusive hosts']);
block(s,4.71,1.72,3.9,3.75,'Execution',['One master (mbd), sbd on each host','Cgroup v2 resource enforcement','Batch jobs and persistent services','Per-instance network namespaces'],C.green);
block(s,8.89,1.72,3.9,3.75,'Recovery and API',['Durable events and recovery','TCP / XDR / HMAC-SHA256','C client API (llbatch)','CLI tools share the same API'],C.gold);
tx(s,'LavaLite runs native batch jobs and long-running services without a second scheduler.',.57,6.01,12.1,.65,20,C.cyan,{bold:true});
s=base('Why persistent services belong in HPC','JupyterLab, code-server and inference services need resources and scheduling, just like batch jobs.',2);
block(s,.55,1.73,5.95,3.62,'Common needs',['Resource allocation and host placement','Isolation and limits','Scheduling policies','State tracking and recovery'],C.cyan);
block(s,6.77,1.73,5.95,3.62,'One scheduler',['A service is represented as a job','The same queues and hosts are used','sbd launches and tracks its processes','mbd records the service endpoint'],C.green);
tx(s,'A persistent service is a workload type, not a separate control plane.',.57,5.86,12.1,.8,22,C.gold,{bold:true});
s=base('A service is a job','Definitions live in llb.services; instances use the existing batch scheduling path.',3);
rect(s,.55,1.64,6.08,4.9,C.panel2);tx(s,'Example service definition',.82,1.91,5.55,.42,20,C.green,{bold:true});
const cfg='Begin Service\nSERVICE_NAME = jupyter\nTYPE         = user\nRUNTIME      = apptainer\nIMAGE        = /path/to/jupyter.sif\nCOMMAND      = jupyter lab --ip=0.0.0.0\nPORT         = 8888\nRESTART      = always\nQUEUE        = service\nEnd Service';
tx(s,cfg,.83,2.48,5.55,3.55,16,C.white,{fontFace:'Aptos Mono',valign:'top',breakLine:false});
block(s,6.87,1.65,5.88,4.88,'Lifecycle',['User starts an instance with bservices','mbd creates and schedules a service job','sbd launches it on an execution host','Host and port are reported to mbd','The manifest supports recovery after restart'],C.cyan);
s=base('Service networking on the execution host','Each running instance has its own Linux network namespace and published TCP endpoint.',4);
rect(s,.55,1.75,3.55,2.7);tx(s,'Client',.84,2.0,2.95,.4,21,C.cyan,{bold:true});tx(s,'Browser or SSH tunnel\nworker:30000',.84,2.63,2.95,1.1,19,C.white);
rect(s,4.89,1.75,3.48,2.7);tx(s,'Worker host',5.12,2.0,3,.4,21,C.gold,{bold:true});tx(s,'nftables DNAT\nveth host end\n10.200.0.1/30',5.12,2.58,2.96,1.32,17,C.white);
rect(s,9.14,1.75,3.6,2.7);tx(s,'Network namespace',9.36,2.0,3.1,.43,20,C.green,{bold:true});tx(s,'svcN  •  veth peer\n10.200.0.2/30\nJupyterLab :8888',9.36,2.58,3.1,1.35,17,C.white);
arrow(s,4.1,3.12,4.82,3.12);arrow(s,8.38,3.12,9.08,3.12);
rect(s,.55,4.9,12.2,1.22,C.panel2);tx(s,'sbd allocates a free endpoint port (30000–46383) and a matching /30 subnet. Outbound traffic uses masquerading.',.85,5.12,11.65,.75,16,C.muted);
s=base('From bservices to the service URL','One request follows the same scheduler and execution path as a batch job.',5);
let steps=[['1','User','bservices jupyter'],['2','mbd','Create service job; schedule on a host'],['3','sbd','Allocate endpoint; set up netns; start runtime'],['4','mbd','Record host, port and running status'],['5','User','Access worker:port (directly or by SSH tunnel)']];
steps.forEach((a,i)=>{let yy=1.62+i*.93;rect(s,.65,yy,12.05,.76,i%2?C.panel2:C.panel);rect(s,.82,yy+.13,.46,.46,C.cyan);tx(s,a[0],.91,yy+.21,.3,.22,14,C.bg,{bold:true});tx(s,a[1],1.57,yy+.13,2.1,.44,18,C.gold,{bold:true});tx(s,a[2],3.55,yy+.11,8.9,.52,16,C.white);});
s=base('A working JupyterLab service','Example host and ports are illustrative; the allocated endpoint is reported by bservices.',6);
rect(s,.65,1.74,7.0,3.87,C.panel2);tx(s,'$ bservices jupyter\nJob <8>: http://worker4:30000/?token=...\n\n$ bservices\nNAME       USER    HOST:PORT       STATUS\njupyter    user    worker4:30000    RUN',.95,2.06,6.4,3.19,17,C.white,{fontFace:'Aptos Mono',valign:'top'});
block(s,7.93,1.74,4.72,3.87,'Access options',['Direct: worker4:30000','Or SSH tunnel through login node','ssh -L 8888:worker4:30000 user@login','Then browse localhost:8888'],C.green);
tx(s,'Instances share the same service PORT inside separate namespaces; host endpoint ports are unique per worker.',.72,6.05,11.98,.6,17,C.muted);
s=base('Design principles','Keep the architecture small and explainable as LavaLite evolves.',7);
block(s,.54,1.72,3.92,3.7,'One scheduler',['Batch jobs and persistent services','Shared resources and queue policy','No second control plane']);
block(s,4.7,1.72,3.92,3.7,'Linux primitives',['Native execution and cgroup v2','Network namespaces, veth, nftables','Apptainer for configured services'],C.green);
block(s,8.86,1.72,3.92,3.7,'Recoverable state',['Durable event manifest','Job and instance recovery','Endpoint discovery by job ID'],C.gold);
tx(s,'Architecture first. Release-specific changes belong in the release notes.',.58,6.0,12,.67,21,C.cyan,{bold:true});
pptx.writeFile({fileName:path.join(outputDir, 'lavalite-architecture.pptx')});

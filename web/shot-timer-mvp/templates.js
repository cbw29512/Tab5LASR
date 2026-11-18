(() => {
  // DATA FIRST: these are ORIGINAL illustrative training shapes, not certified match targets.
  const TEMPLATES = Object.freeze([
    {id:"bullseye", name:"Bullseye", subtitle:"Concentric scoring rings",kind:"rings"},
    {id:"five-dot",name:"Five Dot Drill",subtitle:"Five practice circles",kind:"dots"},
    {id:"silhouette",name:"Training Silhouette",subtitle:"Generic center and outer zones",kind:"silhouette"},
    {id:"plates",name:"Plate Rack",subtitle:"Five round plates",kind:"plates"},
    {id:"blank",name:"Blank Custom",subtitle:"Entire paper counts as one hit",kind:"blank"}
  ]);
  const DOTS = [[.30,.25],[.70,.25],[.50,.50],[.30,.75],[.70,.75]];
  const PLATES = [.16,.33,.50,.67,.84].map(x=>[x,.50]);
  const svgNS = "http://www.w3.org/2000/svg";
  function svg(tag, attrs={}) {
    try {
      const node = document.createElementNS(svgNS,tag);
      Object.entries(attrs).forEach(([key,value])=>node.setAttribute(key,String(value)));
      return node;
    } catch(error) { window.LASR.handleError("Creating target artwork",error); return null; }
  }
  function drawArtwork(id, host) {
    try {
      host.replaceChildren();
      const add = (tag,attrs) => { const node=svg(tag,attrs); if(node)host.appendChild(node); };
      const cx=381.5,cy=199.5,w=307,h=343;
      add("text",{x:cx,y:55,"text-anchor":"middle",fill:"#53616c","font-size":15,
        "font-weight":"bold","letter-spacing":2});
      if(id==="bullseye") {
        [.40,.32,.24,.16,.085].forEach((r,i)=>add("circle",{cx,cy,r:r*w,
          fill:i===4?"#db9a50":i%2?"#23313a":"#e9e6d8",stroke:"#24323c","stroke-width":2}));
      } else if(id==="five-dot" || id==="plates") {
        const centers=id==="five-dot"?DOTS:PLATES;
        centers.forEach(([x,y],i)=>{
          add("circle",{cx:228+x*w,cy:28+y*h,r:id==="five-dot"?27:22,
            fill:"#172c38",stroke:"#a2abb0","stroke-width":3});
          add("circle",{cx:228+x*w,cy:28+y*h,r:5,fill:"#d7a25f"});
        });
      } else if(id==="silhouette") {
        add("path",{d:"M356 87 Q381 72 407 87 L422 111 L455 129 L468 300 L295 300 L308 129 L340 111 Z",
          fill:"#1c2d38",stroke:"#495966","stroke-width":3});
        add("rect",{x:348,y:145,width:67,height:100,rx:15,fill:"none",stroke:"#e5d8b3","stroke-width":3});
        add("circle",{cx,cy:190,r:20,fill:"none",stroke:"#e5d8b3","stroke-width":2});
      } else add("rect",{x:242,y:60,width:279,height:295,fill:"none",stroke:"#485966","stroke-dasharray":"8 8","stroke-width":2});
      add("text",{x:cx,y:350,"text-anchor":"middle",fill:"#5a6770","font-size":13,
        "font-weight":700});
      const texts=host.querySelectorAll("text"); if(texts[0])texts[0].textContent=id.toUpperCase().replaceAll("-"," ");
      if(texts[1])texts[1].textContent="LASR PRACTICE · DEMO";
    } catch(error) { window.LASR.handleError("Drawing template",error); }
  }
  function scoreAt(id,u,v) {
    try {
      if(!Number.isFinite(u)||!Number.isFinite(v)||u<0||u>1||v<0||v>1) return {zone:"MISS",score:0};
      if(id==="blank")return {zone:"PAPER",score:1};
      const r=Math.hypot((u-.5),(v-.5)*343/307);
      if(id==="bullseye") {
        const radii=[.085,.16,.24,.32,.40],values=[10,9,8,7,5];
        const index=radii.findIndex(x=>r<=x);return index<0?{zone:"MISS",score:0}:{zone:`R${index+1}`,score:values[index]};
      }
      if(id==="five-dot"||id==="plates") {
        const dots=id==="five-dot"?DOTS:PLATES;
        const index=dots.findIndex(([x,y])=>Math.hypot((u-x),(v-y)*343/307)<=(id==="five-dot"?.09:.075));
        return index<0?{zone:"MISS",score:0}:{zone:`DOT ${index+1}`,score:1};
      }
      if(id==="silhouette") {
        if(u>=.39&&u<=.61&&v>=.34&&v<=.64)return {zone:"CENTER",score:5};
        return (u>=.24&&u<=.76&&v>=.25&&v<=.80)?{zone:"OUTER",score:3}:{zone:"MISS",score:0};
      }
      return {zone:"MISS",score:0};
    } catch(error) { window.LASR.handleError("Scoring shot",error);return {zone:"ERROR",score:0}; }
  }
  window.LASRTemplates = {TEMPLATES,drawArtwork,scoreAt,svg};
})();
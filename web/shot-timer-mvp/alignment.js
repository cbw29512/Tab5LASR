(() => {
  const {settings,session,PHASES,handleError} = window.LASR;
  let activeCorner=null;
  function svgPoint(event) {
    try {
      const svg=document.getElementById("targetSvg"),pt=svg.createSVGPoint();
      pt.x=event.clientX;pt.y=event.clientY;
      return pt.matrixTransform(svg.getScreenCTM().inverse());
    } catch(error) {handleError("Reading preview coordinates",error);return null;}
  }
  function project(u,v) {
    try {
      const [a,b,c,d]=settings.corners;
      return {x:(1-u)*(1-v)*a[0]+u*(1-v)*b[0]+u*v*c[0]+(1-u)*v*d[0],
        y:(1-u)*(1-v)*a[1]+u*(1-v)*b[1]+u*v*c[1]+(1-u)*v*d[1]};
    } catch(error){handleError("Projecting target point",error);return {x:0,y:0};}
  }
  function unproject(x,y) {
    try {
      let u=.5,v=.5;
      // Newton inverse of the bilinear map: perspective-safe enough for this demo;
      // real camera scoring requires a calibrated homography instead.
      const [a,b,c,d]=settings.corners;
      for(let i=0;i<12;i++) {
        const p=project(u,v),dx=x-p.x,dy=y-p.y;
        const ux=(1-v)*(b[0]-a[0])+v*(c[0]-d[0]);
        const uy=(1-v)*(b[1]-a[1])+v*(c[1]-d[1]);
        const vx=(1-u)*(d[0]-a[0])+u*(c[0]-b[0]);
        const vy=(1-u)*(d[1]-a[1])+u*(c[1]-b[1]);
        const det=ux*vy-uy*vx;
        if(Math.abs(det)<1e-6)return null;
        u+=(dx*vy-dy*vx)/det;v+=(ux*dy-uy*dx)/det;
      }
      return {u,v};
    } catch(error){handleError("Converting shot position",error);return null;}
  }
  function init(onChanged) {
    try {
      const svg=document.getElementById("targetSvg");
      svg.addEventListener("pointerdown",event=>{
        try {
          const handle=event.target.closest?.("[data-corner]");
          if(!handle||![PHASES.CHECK,PHASES.READY,PHASES.COMPLETE].includes(session.phase))return;
          activeCorner=Number(handle.dataset.corner);
          if(session.phase===PHASES.COMPLETE)return;
          svg.setPointerCapture(event.pointerId);event.preventDefault();
        } catch(error){handleError("Starting corner drag",error);}
      });
      svg.addEventListener("pointermove",event=>{
        try {
          if(activeCorner===null)return;
          const p=svgPoint(event);if(!p)return;
          settings.corners[activeCorner]=[Math.max(12,Math.min(748,p.x)),Math.max(12,Math.min(398,p.y))];
          session.alignmentConfirmed=false;session.phase=PHASES.CHECK;settings.savedAt=null;
          onChanged();
        } catch(error){handleError("Dragging corner",error);}
      });
      const release=()=>{activeCorner=null;};
      svg.addEventListener("pointerup",release);svg.addEventListener("pointercancel",release);
    } catch(error){handleError("Configuring alignment control",error);}
  }
  window.LASRAlignment={svgPoint,project,unproject,init};
})();
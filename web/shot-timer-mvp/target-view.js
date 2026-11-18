(() => {
  const {session,settings,handleError}=window.LASR;
  const {TEMPLATES,drawArtwork,svg}=window.LASRTemplates;
  function renderTarget() {
    try {
      drawArtwork(settings.activeTemplateId,document.getElementById("targetArtwork"));
      const points=settings.corners;
      document.getElementById("targetOutline").setAttribute("d",`M${points.map(p=>p.join(" ")).join(" L")} Z`);
      const host=document.getElementById("cornerHandles");host.replaceChildren();
      points.forEach(([x,y],i)=>{
        const c=svg("circle",{cx:x,cy:y,r:10,fill:"#6ee0b3",stroke:"#0a382a","stroke-width":3,
          "data-corner":i,role:"button","aria-label":`Drag target corner ${i+1}`});
        if(c)host.appendChild(c);
      });
      const dots=document.getElementById("shotMarkers");dots.replaceChildren();
      session.shots.forEach((shot,index)=>{
        const c=svg("circle",{cx:shot.x,cy:shot.y,r:index===session.shots.length-1?9:5,
          fill:shot.score?"#ff9b60":"#f05959",stroke:"#482318","stroke-width":2});
        if(c)dots.appendChild(c);
      });
    } catch(error) {handleError("Updating target preview",error);}
  }
  function renderLibrary(chooseTemplate) {
    try {
      const host=document.getElementById("templateGrid");host.replaceChildren();
      TEMPLATES.forEach(t=>{
        const button=document.createElement("button");button.type="button";
        button.className="template-card"+(settings.activeTemplateId===t.id?" selected":"");
        const thumbnail=document.createElementNS("http://www.w3.org/2000/svg","svg");
        thumbnail.setAttribute("viewBox","200 0 360 400");
        const bg=svg("rect",{x:228,y:28,width:307,height:343,rx:4,fill:"#eae6d9"});
        thumbnail.appendChild(bg);
        const art=svg("g");thumbnail.appendChild(art);drawArtwork(t.id,art);
        const title=document.createElement("strong");title.textContent=t.name;
        const subtitle=document.createElement("span");subtitle.textContent=t.subtitle;
        button.append(thumbnail,title,subtitle);
        const note=document.createElement("small");note.textContent=settings.profiles[t.id]?"SAVED ALIGNMENT":"DEFAULT ALIGNMENT";
        button.appendChild(note);
        button.addEventListener("click",()=>chooseTemplate(t.id));host.appendChild(button);
      });
    } catch(error) {handleError("Rendering target library",error);}
  }
  window.LASRTargetView={renderTarget,renderLibrary};
})();
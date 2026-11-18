(() => {
  const {settings,DEFAULT_CORNERS,handleError} = window.LASR;
  const KEY="tab5-lasr-shot-timer-mvp-v1";
  const templateIds=new Set(window.LASRTemplates.TEMPLATES.map(t=>t.id));
  const validCorners = corners => Array.isArray(corners)&&corners.length===4&&corners.every(
    p=>Array.isArray(p)&&p.length===2&&Number.isFinite(p[0])&&Number.isFinite(p[1])&&
    p[0]>=10&&p[0]<=750&&p[1]>=10&&p[1]<=400);
  function load() {
    try {
      const raw=localStorage.getItem(KEY);
      settings.persistent=true;
      if(!raw)return false;
      const saved=JSON.parse(raw);
      if(saved.schemaVersion!==1||!templateIds.has(saved.activeTemplateId))throw Error("Invalid profile version or target");
      settings.activeTemplateId=saved.activeTemplateId;
      settings.profiles={};
      for(const [id,profile] of Object.entries(saved.profiles||{})) {
        if(templateIds.has(id)&&validCorners(profile.corners))settings.profiles[id]={
          corners:profile.corners.map(p=>[...p]),savedAt:String(profile.savedAt||"")};
      }
      activate(settings.activeTemplateId);
      return true;
    } catch(error) { console.warn("[LASR MVP] Profile not restored; defaults used",error); settings.persistent=false;return false; }
  }
  function activate(id) {
    try {
      if(!templateIds.has(id))throw Error("Unknown template");
      settings.activeTemplateId=id;
      const saved=settings.profiles[id];
      settings.corners=(saved?.corners||DEFAULT_CORNERS).map(p=>[...p]);
      settings.savedAt=saved?.savedAt||null;
      persist();
    } catch(error) {handleError("Selecting target profile",error);}
  }
  function persist() {
    try {
      const data={schemaVersion:1,activeTemplateId:settings.activeTemplateId,profiles:settings.profiles};
      localStorage.setItem(KEY,JSON.stringify(data));
      settings.persistent=true;return true;
    } catch(error) { console.warn("[LASR MVP] Local storage unavailable; profile is temporary",error);settings.persistent=false;return false; }
  }
  function saveCurrent() {
    try {
      if(!validCorners(settings.corners))throw Error("Invalid corner coordinates");
      const area=Math.abs(settings.corners.reduce((s,p,i)=>{const q=settings.corners[(i+1)%4];return s+p[0]*q[1]-q[0]*p[1];},0))/2;
      if(area<5000)throw Error("Target outline is too small or self-crossing");
      const savedAt=new Date().toISOString();
      settings.profiles[settings.activeTemplateId]={corners:settings.corners.map(p=>[...p]),savedAt};
      settings.savedAt=savedAt;
      return persist();
    } catch(error) {handleError("Saving target alignment",error);return false;}
  }
  window.LASRStorage={KEY,load,activate,saveCurrent,persist,validCorners};
})();
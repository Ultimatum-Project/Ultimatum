const params=new URLSearchParams(location.search);
const frame=document.querySelector("#responsiveApp");
if(params.get("layout")==="landscape") {frame.style.width="844px";frame.style.height="390px";}
frame.src=params.get("surface")==="home" ? "/runtime/home.html" : "/runtime/index.html";
if(params.get("audit")==="1") frame.src+="?audit=1";

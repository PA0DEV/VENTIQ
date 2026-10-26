#include "WebUpdater.h"
#include "FirmwareInfo.h"

void WebUpdater::begin(ESP8266WebServer &server, UpdateManager &mgr) {
  server_ = &server;
  mgr_ = &mgr;
  server.on("/api/update", HTTP_POST, [this]() { onUploadDone(); }, [this]() { onUpload(); });
  server.on("/api/update/status", HTTP_GET, [this]() { sendStatus(200); });
  server.on("/api/firmware", HTTP_GET, [this]() {
    String s = String("{\"version\":\"") + FirmwareInfo::versionString() + "\",\"hardware\":\"" +
               FirmwareInfo::hardware() + "\"}";
    server_->send(200, "application/json", s);
  });
}

void WebUpdater::onUpload() {
  HTTPUpload &up = server_->upload();
  switch (up.status) {
    case UPLOAD_FILE_START:
      owner_ = mgr_->start(server_->arg("sha256").c_str());
      break;
    case UPLOAD_FILE_WRITE:
      if (owner_) mgr_->write(up.buf, up.currentSize);
      break;
    case UPLOAD_FILE_END:
      if (owner_) mgr_->finish();
      owner_ = false;
      break;
    case UPLOAD_FILE_ABORTED:
      if (owner_) mgr_->abort(UpdateError::Aborted);
      owner_ = false;
      break;
  }
}

void WebUpdater::onUploadDone() {
  UpdateStatus st = mgr_->status();
  int code = 200;
  if (st.state == UpdateState::Failed) {
    code = (st.error == UpdateError::WriteFailed || st.error == UpdateError::FinalizeFailed) ? 500 : 400;
  } else if (st.state != UpdateState::RebootPending) {
    code = 409;  // z. B. anderer Update laeuft bereits
  }
  sendStatus(code);
}

void WebUpdater::sendStatus(int code) {
  UpdateStatus st = mgr_->status();
  String s = String("{\"state\":\"") + updateStateName(st.state) + "\",\"error\":\"" +
             updateErrorName(st.error) + "\",\"received\":" + st.received +
             ",\"capacity\":" + st.capacity + "}";
  server_->send(code, "application/json", s);
}

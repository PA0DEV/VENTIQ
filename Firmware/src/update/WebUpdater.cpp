#include "WebUpdater.h"
#include "FirmwareInfo.h"
#include <string.h>

void WebUpdater::begin(ESP8266WebServer &server, UpdateManager &mgr, BundleUpdater &bundle) {
  server_ = &server;
  mgr_ = &mgr;
  bundle_ = &bundle;
  server.on("/api/update", HTTP_POST, [this]() { onUploadDone(); }, [this]() { onUpload(); });
  server.on("/api/update/bundle", HTTP_POST,
            [this]() { onBundleUploadDone(); }, [this]() { onBundleUpload(); });
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

void WebUpdater::onBundleUpload() {
  HTTPUpload &upload = server_->upload();
  switch (upload.status) {
    case UPLOAD_FILE_START:
      reportBundleStatus_ = true;
      bundleOwner_ = false;
      bundleRequestFailed_ = false;
      bundleRequestError_ = "bad_bundle";
      {
        String filename = upload.filename;
        filename.toLowerCase();
        if (!filename.endsWith(".vtu")) {
          bundleRequestFailed_ = true;
          bundleRequestError_ = "bad_file_type";
          return;
        }
      }
      bundleOwner_ = bundle_->start();
      if (!bundleOwner_) {
        bundleRequestFailed_ = true;
        bundleRequestError_ = bundle_->status().error;
      }
      break;
    case UPLOAD_FILE_WRITE:
      if (bundleOwner_ && !bundle_->write(upload.buf, upload.currentSize)) {
        bundleOwner_ = false;
        bundleRequestFailed_ = true;
        bundleRequestError_ = bundle_->status().error;
      }
      break;
    case UPLOAD_FILE_END:
      if (bundleOwner_ && !bundle_->finish()) {
        bundleRequestFailed_ = true;
        bundleRequestError_ = bundle_->status().error;
      }
      bundleOwner_ = false;
      break;
    case UPLOAD_FILE_ABORTED:
      if (bundleOwner_) bundle_->abort(UpdateError::Aborted);
      bundleOwner_ = false;
      bundleRequestFailed_ = true;
      bundleRequestError_ = "aborted";
      break;
    default:
      break;
  }
}

void WebUpdater::onUploadDone() {
  reportBundleStatus_ = false;
  UpdateStatus st = mgr_->status();
  int code = 200;
  if (st.state == UpdateState::Failed) {
    code = (st.error == UpdateError::WriteFailed || st.error == UpdateError::FinalizeFailed) ? 500 : 400;
  } else if (st.state != UpdateState::RebootPending) {
    code = 409;  // z. B. anderer Update laeuft bereits
  }
  sendStatus(code);
}

void WebUpdater::onBundleUploadDone() {
  reportBundleStatus_ = true;
  int code = 200;
  if (bundleRequestFailed_) {
    if (strcmp(bundleRequestError_, "busy") == 0) code = 409;
    else if (strcmp(bundleRequestError_, "filesystem_unavailable") == 0) code = 503;
    else if (strcmp(bundleRequestError_, "no_space") == 0 ||
             strcmp(bundleRequestError_, "asset_no_space") == 0) code = 507;
    else if (strcmp(bundleRequestError_, "asset_write_failed") == 0 ||
             strcmp(bundleRequestError_, "write_failed") == 0 ||
             strcmp(bundleRequestError_, "finalize_failed") == 0) code = 500;
    else code = 400;
  } else if (mgr_->status().state != UpdateState::RebootPending) {
    code = 409;
  }
  sendStatus(code);
}

void WebUpdater::sendStatus(int code) {
  UpdateStatus st = mgr_->status();
  String s = String("{\"state\":\"") + updateStateName(st.state) + "\",\"error\":\"" +
             updateErrorName(st.error) + "\",\"received\":" + st.received +
             ",\"capacity\":" + st.capacity + "}";
  if (reportBundleStatus_) {
    const BundleStatus bundle = bundle_->status();
    s.remove(s.length() - 1);
    s += String(",\"kind\":\"bundle\",\"version\":\"") + bundle.version +
         "\",\"bundle\":{\"assetsTotal\":" + bundle.assetsTotal +
         ",\"assetsDone\":" + bundle.assetsDone +
         ",\"bytes\":" + bundle.bytesReceived +
         ",\"expected\":" + bundle.bytesExpected +
         ",\"assetsPending\":" + (bundle.assetsPending ? "true" : "false") + "}}";
    if (bundleRequestFailed_ && st.state != UpdateState::RebootPending) {
      s.replace(String("\"error\":\"") + updateErrorName(st.error) + "\"",
                String("\"error\":\"") + bundleRequestError_ + "\"");
    } else if (bundle.error[0] != '\0' && strcmp(bundle.error, "none") != 0) {
      s.replace("\"error\":\"none\"", String("\"error\":\"") + bundle.error + "\"");
    }
  }
  server_->send(code, "application/json", s);
}

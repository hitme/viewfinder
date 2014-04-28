// Copyright 2013 Viewfinder. All rights reserved.
// Author: Peter Mattis.

#import "AuthMetadata.pb.h"
#import "Callback.h"
#import "DB.h"
#import "DBFormat.h"
#import "ScopedPtr.h"
#import "SystemMessage.pb.h"
#import "WallTime.h"

class ActivityTable;
class Analytics;
class AsyncState;
class Breadcrumb;
class CommentTable;
class ContactManager;
class DayTable;
class DayTableEnv;
class EpisodeTable;
class GeocodeManager;
class ImageIndex;
class NetworkManager;
class NetworkQueue;
class NotificationManager;
class PeopleRank;
class PhotoStorage;
class PhotoTable;
class PlacemarkHistogram;
class PlacemarkTable;
class SubscriptionManager;
class ViewpointTable;

typedef CallbackSet1<bool> MaintenanceDone;
typedef CallbackSet1<const string> MaintenanceProgress;
typedef CallbackSet1<bool> SettingsChanged;
typedef Callback<void (const string)> ProgressUpdateBlock;

class AppState {
 public:
  static const string kLinkEndpoint;
  static const string kLoginEndpoint;
  static const string kLoginResetEndpoint;
  static const string kMergeTokenEndpoint;
  static const string kRegisterEndpoint;
  static const string kVerifyEndpoint;

  enum InitAction {
    INIT_NORMAL,
    INIT_FSCK,
    INIT_RESET,
  };

  // Protocol versions.
  enum ProtocolVersion {
    INITIAL_VERSION = 0,
    ADD_HEADERS_VERSION = 1,
    TEST_VERSION = 2,
    RENAME_EVENT_VERSION = 3,
    ADD_TO_VIEWPOINT_VERSION = 4,
    QUERY_EPISODES_VERSION = 5,
    UPDATE_POST_VERSION = 6,
    UPDATE_SHARE_VERSION = 7,
    ADD_OP_HEADER_VERSION = 8,
    ADD_ACTIVITY_VERSION = 9,
    EXTRACT_MD5_HASHES = 10,
    INLINE_INVALIDATIONS = 11,
    EXTRACT_FILE_SIZES = 12,
    INLINE_COMMENTS = 13,
    EXTRACT_ASSET_KEYS = 14,
    SPLIT_NAMES = 15,
    EXPLICIT_SHARE_ORDER = 16,
    SUPPRESS_BLANK_COVER_PHOTO = 17,
    SUPPORT_MULTIPLE_IDENTITIES_PER_CONTACT = 18,
    RENAME_PHOTO_LABEL = 19,
    SUPPRESS_AUTH_NAME = 20,
    SEND_EMAIL_TOKEN = 21,
    SUPPORT_REMOVED_FOLLOWERS = 22,
    SUPPRESS_COPY_TIMESTAMP = 23,
    SUPPORT_CONTACT_LIMITS = 24,
    SUPPRESS_EMPTY_TITLE = 25,
  };

  static ProtocolVersion protocol_version() { return SUPPRESS_EMPTY_TITLE; }

 public:
  AppState(const string& base_dir, const string& server_host,
           int server_port, bool production);
  virtual ~AppState();

  virtual InitAction GetInitAction() = 0;
  virtual bool Init(InitAction init_action);
  virtual void RunMaintenance(InitAction init_action);

  // Setup commit trigger that adds an update callback looking for the
  // specified "viewpoint_id".
  virtual void SetupViewpointTransition(int64_t viewpoint_id, const DBHandle& updates) = 0;

  // Returns true if a new device id needs to be generated by the
  // Viewfinder backend. This happens when the physical device
  // changes (e.g. as on a backup/restore to a new device).
  bool NeedDeviceIdReset() const;

  // Returns true if the user has enabled cloud storage and has a sufficient
  // subscription to do so.
  virtual bool CloudStorageEnabled() = 0;

  // Delete the specified asset.
  virtual void DeleteAsset(const string& key) = 0;

  // Process the duplicate photo queue.
  virtual void ProcessPhotoDuplicateQueue() = 0;

  // Generates the viewfinder images for the specified photo, invoking
  // "completion" when done.
  virtual void LoadViewfinderImages(int64_t photo_id, const DBHandle& db,
                                    Callback<void (bool)> completion) = 0;

  // Returns the seconds from GMT for the current time zone at the specified
  // date.
  virtual int TimeZoneOffset(WallTime t) const = 0;

  void SetUserAndDeviceId(int64_t user_id, int64_t device_id);
  void SetDeviceId(int64_t v) {
    SetUserAndDeviceId(auth_.user_id(), v);
  }
  void SetUserId(int64_t v) {
    SetUserAndDeviceId(v, auth_.device_id());
  }
  void SetAuthCookies(const string& user_cookie, const string& xsrf_cookie);

  // Clear user/device id and cookies. Used when logging out.
  void ClearAuthMetadata();

  // Provides a client-local monotonic sequence for operation
  // ids. These should be stored with ServerOperation protobufs for
  // use with JSON service requests. The local operation ids should be
  // used to encode activity ids corresponding to each server
  // operation. This allows a non-connected client to generate
  // activities locally which will be linkable to server-side
  // activities when eventual connectivity allows the operation to run
  // and resultant notifications to be queried.
  int64_t NewLocalOperationId();

  // Creates a transactional database handle. Data written or deleted
  // during the transaction will be visible when using the DBHandle
  // for access. However, the data will not be visible from db() until
  // the transaction is committed. No locking is provided by the
  // underlying database, so the most recent write will always "win"
  // in the case of concurrent write ops to the same key.
  DBHandle NewDBTransaction();

  // Creates a snapshot of the underlying database. The snapshot will be
  // valid as long as a reference exists to this handle. No mutating
  // database calls are allowed to the returned db handle.
  DBHandle NewDBSnapshot();

  virtual WallTime WallTime_Now() { return ::WallTime_Now(); }

  const DBHandle& db() const { return db_; }

  ActivityTable* activity_table() const { return activity_table_.get(); }
  Analytics* analytics() { return analytics_.get(); }
  AsyncState* async() { return async_.get(); }
  CommentTable* comment_table() const { return comment_table_.get(); }
  ContactManager* contact_manager() { return contact_manager_.get(); }
  DayTable* day_table() const { return day_table_.get(); }
  EpisodeTable* episode_table() const { return episode_table_.get(); }
  GeocodeManager* geocode_manager() const { return geocode_manager_.get(); }
  ImageIndex* image_index() const { return image_index_.get(); }
  NetworkManager* net_manager() const { return net_manager_.get(); }
  NetworkQueue* net_queue() const { return net_queue_.get(); }
  NotificationManager* notification_manager() const { return notification_manager_.get(); }
  PeopleRank* people_rank() const { return people_rank_.get(); }
  PhotoStorage* photo_storage() const { return photo_storage_.get(); }
  PhotoTable* photo_table() const { return photo_table_.get(); }
  PlacemarkHistogram* placemark_histogram() const { return placemark_histogram_.get(); }
  PlacemarkTable* placemark_table() const { return placemark_table_.get(); }
  virtual SubscriptionManager* subscription_manager() const = 0;
  ViewpointTable* viewpoint_table() const { return viewpoint_table_.get(); }

  const string& server_protocol() const { return server_protocol_; }
  const string& server_host() const { return server_host_; }
  void set_server_host(const Slice& host);
  int server_port() const { return server_port_; }

  const string& photo_dir() const { return photo_dir_; }
  const string& server_photo_dir() const { return server_photo_dir_; }

  bool is_registered() const { return !fake_logout_ && auth_.user_id(); }
  int64_t device_id() const { return auth_.device_id(); }
  int64_t user_id() const { return auth_.user_id(); }
  string device_uuid() const { return device_uuid_; }
  const AuthMetadata& auth() const { return auth_; }

  const Breadcrumb* last_breadcrumb() const { return last_breadcrumb_.get(); }
  void set_last_breadcrumb(const Breadcrumb& b);

  // cloud_storage() is the value of the user's cloud storage preference.
  // This preference should be ignored unless the user has a sufficient
  // subscription; use CloudStorageEnabled() instead of accessing this
  // value directly in most cases.
  bool cloud_storage() const { return cloud_storage_; }
  void set_cloud_storage(bool v);

  bool store_originals() const { return store_originals_; }
  void set_store_originals(bool v);

  bool no_password() const { return no_password_; }
  void set_no_password(bool v);

  // Returns true if at least one full refresh has completed since user authentication.
  bool refresh_completed() { return refresh_completed_; }
  void set_refresh_completed(bool v);

  bool upload_logs() const { return upload_logs_; }
  void set_upload_logs(bool v);

  WallTime last_login_timestamp() const { return last_login_timestamp_; }
  void set_last_login_timestamp(WallTime v);

  enum RegistrationVersion {
    REGISTRATION_GOOGLE_FACEBOOK,
    REGISTRATION_EMAIL,
  };
  static RegistrationVersion current_registration_version() {
    return REGISTRATION_EMAIL;
  }
  RegistrationVersion registration_version() const { return registration_version_; }
  void set_registration_version(RegistrationVersion v);

  const SystemMessage& system_message() const { return system_message_; }
  void clear_system_message();
  void set_system_message(const SystemMessage& msg);

  bool account_setup() const { return account_setup_; }
  void set_account_setup(bool account_setup) { account_setup_ = account_setup; }

  MaintenanceDone* maintenance_done() { return &maintenance_done_; }
  MaintenanceProgress* maintenance_progress() { return &maintenance_progress_; }
  CallbackSet1<int>* network_ready() { return &network_ready_; }
  CallbackSet* app_did_become_active() { return &app_did_become_active_; }
  CallbackSet* app_will_resign_active() { return &app_will_resign_active_; }
  // Callbacks for settings changed or downloaded.
  // Takes a bool as argument. This bool is true if run after
  // settings were downloaded from the server, in which case they
  // should be applied but not uploaded again.
  SettingsChanged* settings_changed() { return &settings_changed_; }
  CallbackSet* system_message_changed() { return &system_message_changed_; }

  const string& device_model() const { return device_model_; }
  const string& device_name() const { return device_name_; }
  const string& device_os() const { return device_os_; }

  const string& locale_language() const { return locale_language_; }
  const string& locale_country() const { return locale_country_; }
  const string& test_udid() const { return test_udid_; }

  virtual bool network_wifi() const;
  virtual string timezone() const = 0;

 protected:
  void Kill();
  bool OpenDB(bool reset);
  bool InitDB();
  void InitDirs();
  virtual void InitVars();
  virtual DayTableEnv* NewDayTableEnv() = 0;
  virtual void Clean(const string& dir);
  virtual bool MaybeMigrate(ProgressUpdateBlock progress_update) = 0;
  bool MaybeFSCK(bool force, ProgressUpdateBlock progress_update);
  void WriteAuthMetadata();

 protected:
  const string server_protocol_;
  string server_host_;
  const int server_port_;
  const string base_dir_;
  const string library_dir_;
  const string database_dir_;
  const string photo_dir_;
  const string server_photo_dir_;
  const string auth_path_;
  AuthMetadata auth_;
  ScopedPtr<Breadcrumb> last_breadcrumb_;
  const bool production_;
  string device_uuid_;
  bool cloud_storage_;
  bool store_originals_;
  bool no_password_;
  bool initial_contact_import_done_;
  bool refresh_completed_;
  bool upload_logs_;
  bool account_setup_;
  WallTime last_login_timestamp_;
  RegistrationVersion registration_version_;
  SystemMessage system_message_;
  string device_model_;
  string device_name_;
  string device_os_;
  string locale_language_;
  string locale_country_;
  string test_udid_;
  MaintenanceDone maintenance_done_;
  MaintenanceProgress maintenance_progress_;
  CallbackSet1<int> network_ready_;
  CallbackSet app_did_become_active_;
  CallbackSet app_will_resign_active_;
  SettingsChanged settings_changed_;
  CallbackSet system_message_changed_;
  DBHandle db_;
  ScopedPtr<ActivityTable> activity_table_;
  ScopedPtr<Analytics> analytics_;
  ScopedPtr<AsyncState> async_;
  ScopedPtr<CommentTable> comment_table_;
  ScopedPtr<ContactManager> contact_manager_;
  ScopedPtr<DayTable> day_table_;
  ScopedPtr<EpisodeTable> episode_table_;
  ScopedPtr<GeocodeManager> geocode_manager_;
  ScopedPtr<ImageIndex> image_index_;
  ScopedPtr<NetworkManager> net_manager_;
  ScopedPtr<NetworkQueue> net_queue_;
  ScopedPtr<NotificationManager> notification_manager_;
  ScopedPtr<PeopleRank> people_rank_;
  ScopedPtr<PhotoStorage> photo_storage_;
  ScopedPtr<PhotoTable> photo_table_;
  ScopedPtr<PlacemarkHistogram> placemark_histogram_;
  ScopedPtr<PlacemarkTable> placemark_table_;
  ScopedPtr<ViewpointTable> viewpoint_table_;
  int64_t next_op_id_;
  mutable Mutex next_op_id_mu_;
  bool fake_logout_;
};

// local variables:
// mode: c++
// end:

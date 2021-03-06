#include "../Parser/parser.h"
#include "../Catalog/Catalog.h"
#include "../Catalog/DBObject.h"
#include "../DataMgr/DataMgr.h"
#include "../QueryRunner/QueryRunner.h"
#include <boost/filesystem/operations.hpp>
#include <gtest/gtest.h>
#include <glog/logging.h>
#include <thread>

#ifndef BASE_PATH
#define BASE_PATH "./tmp"
#endif

#define CALCITEPORT 39093

namespace {
std::shared_ptr<Calcite> g_calcite;
std::unique_ptr<Catalog_Namespace::SessionInfo> g_session;
Catalog_Namespace::SysCatalog& sys_cat = Catalog_Namespace::SysCatalog::instance();
Catalog_Namespace::UserMetadata user;
Catalog_Namespace::DBMetadata db;
std::vector<DBObject> privObjects;

class DBObjectPermissionsEnv : public ::testing::Environment {
 public:
  virtual void SetUp() {
    std::string db_name{MAPD_SYSTEM_DB};
    std::string user_name{MAPD_ROOT_USER};
    boost::filesystem::path base_path{BASE_PATH};
    if (!boost::filesystem::exists(base_path)) {
      boost::filesystem::create_directory(base_path);
    }
    CHECK(boost::filesystem::exists(base_path));
    auto system_db_file = base_path / "mapd_catalogs" / "mapd";
    auto data_dir = base_path / "mapd_data";
    g_calcite = std::make_shared<Calcite>(-1, CALCITEPORT, base_path.string(), 1024);
    {
      auto dataMgr = std::make_shared<Data_Namespace::DataMgr>(data_dir.string(), 0, false, 0);
      CHECK(boost::filesystem::exists(system_db_file));
      sys_cat.init(base_path.string(), dataMgr, {}, g_calcite, false, true);
      CHECK(sys_cat.getMetadataForDB(db_name, db));
      auto cat = Catalog_Namespace::Catalog::get(db_name);
      if (cat == nullptr) {
        cat = std::make_shared<Catalog_Namespace::Catalog>(
            base_path.string(), db, dataMgr, std::vector<LeafHostInfo>{}, g_calcite);
        Catalog_Namespace::Catalog::set(db_name, cat);
      }
      CHECK(sys_cat.getMetadataForUser(MAPD_ROOT_USER, user));
    }
    auto dataMgr = std::make_shared<Data_Namespace::DataMgr>(data_dir.string(), 0, false, 0);
    g_session.reset(
        new Catalog_Namespace::SessionInfo(std::make_shared<Catalog_Namespace::Catalog>(
                                               base_path.string(), db, dataMgr, std::vector<LeafHostInfo>{}, g_calcite),
                                           user,
                                           ExecutorDeviceType::GPU,
                                           ""));
  }
};

inline void run_ddl_statement(const std::string& query) {
  QueryRunner::run_ddl_statement(query, g_session);
}
}  // namespace

struct Users {
  void setup_users() {
    if (!sys_cat.getMetadataForUser("Chelsea", user)) {
      sys_cat.createUser("Chelsea", "password", true);
      CHECK(sys_cat.getMetadataForUser("Chelsea", user));
    }
    if (!sys_cat.getMetadataForUser("Arsenal", user)) {
      sys_cat.createUser("Arsenal", "password", false);
      CHECK(sys_cat.getMetadataForUser("Arsenal", user));
    }
    if (!sys_cat.getMetadataForUser("Juventus", user)) {
      sys_cat.createUser("Juventus", "password", false);
      CHECK(sys_cat.getMetadataForUser("Juventus", user));
    }
    if (!sys_cat.getMetadataForUser("Bayern", user)) {
      sys_cat.createUser("Bayern", "password", false);
      CHECK(sys_cat.getMetadataForUser("Bayern", user));
    }
  }
  void drop_users() {
    if (sys_cat.getMetadataForUser("Chelsea", user)) {
      sys_cat.dropUser("Chelsea");
      CHECK(!sys_cat.getMetadataForUser("Chelsea", user));
    }
    if (sys_cat.getMetadataForUser("Arsenal", user)) {
      sys_cat.dropUser("Arsenal");
      CHECK(!sys_cat.getMetadataForUser("Arsenal", user));
    }
    if (sys_cat.getMetadataForUser("Juventus", user)) {
      sys_cat.dropUser("Juventus");
      CHECK(!sys_cat.getMetadataForUser("Juventus", user));
    }
    if (sys_cat.getMetadataForUser("Bayern", user)) {
      sys_cat.dropUser("Bayern");
      CHECK(!sys_cat.getMetadataForUser("Bayern", user));
    }
  }
  Users() { setup_users(); }
  virtual ~Users() { drop_users(); }
};
struct Roles {
  void setup_roles() {
    if (!sys_cat.getMetadataForRole("OldLady")) {
      sys_cat.createRole("OldLady", false);
      CHECK(sys_cat.getMetadataForRole("OldLady"));
    }
    if (!sys_cat.getMetadataForRole("Gunners")) {
      sys_cat.createRole("Gunners", false);
      CHECK(sys_cat.getMetadataForRole("Gunners"));
    }
    if (!sys_cat.getMetadataForRole("Sudens")) {
      sys_cat.createRole("Sudens", false);
      CHECK(sys_cat.getMetadataForRole("Sudens"));
    }
  }

  void drop_roles() {
    if (sys_cat.getMetadataForRole("OldLady")) {
      sys_cat.dropRole("OldLady");
      CHECK(!sys_cat.getMetadataForRole("OldLady"));
    }
    if (sys_cat.getMetadataForRole("Gunners")) {
      sys_cat.dropRole("Gunners");
      CHECK(!sys_cat.getMetadataForRole("Gunners"));
    }
    if (sys_cat.getMetadataForRole("Sudens")) {
      sys_cat.dropRole("Sudens");
      CHECK(!sys_cat.getMetadataForRole("sudens"));
    }
  }
  Roles() { setup_roles(); }
  virtual ~Roles() { drop_roles(); }
};

struct DatabaseObject : testing::Test {
  Catalog_Namespace::UserMetadata user_meta;
  Catalog_Namespace::DBMetadata db_meta;
  Users user_;
  Roles role_;

  explicit DatabaseObject() {}
  virtual ~DatabaseObject() {}
};

struct TableObject : testing::Test {
  const std::string cquery1 = "CREATE TABLE IF NOT EXISTS epl(gp SMALLINT, won SMALLINT);";
  const std::string cquery2 = "CREATE TABLE IF NOT EXISTS seriea(gp SMALLINT, won SMALLINT);";
  const std::string cquery3 = "CREATE TABLE IF NOT EXISTS bundesliga(gp SMALLINT, won SMALLINT);";
  const std::string dquery1 = "DROP TABLE IF EXISTS epl;";
  const std::string dquery2 = "DROP TABLE IF EXISTS seriea;";
  const std::string dquery3 = "DROP TABLE IF EXISTS bundesliga;";
  Users user_;
  Roles role_;

  void setup_tables() {
    run_ddl_statement(cquery1);
    run_ddl_statement(cquery2);
    run_ddl_statement(cquery3);
  }
  void drop_tables() {
    run_ddl_statement(dquery1);
    run_ddl_statement(dquery2);
    run_ddl_statement(dquery3);
  }
  explicit TableObject() {
    drop_tables();
    setup_tables();
  }
  virtual ~TableObject() { drop_tables(); }
};

struct ViewObject : testing::Test {
  void setup_objects() {
    run_ddl_statement("CREATE USER bob (password = 'password', is_super = 'false');");
    run_ddl_statement("CREATE ROLE salesDept;");
    run_ddl_statement("CREATE USER foo (password = 'password', is_super = 'false');");
    run_ddl_statement("GRANT salesDept TO foo;");

    run_ddl_statement("CREATE TABLE bill_table(id integer);");
    run_ddl_statement("CREATE VIEW bill_view AS SELECT id FROM bill_table;");
    run_ddl_statement("CREATE VIEW bill_view_outer AS SELECT id FROM bill_view;");
  }

  void remove_objects() {
    run_ddl_statement("DROP VIEW bill_view_outer;");
    run_ddl_statement("DROP VIEW bill_view;");
    run_ddl_statement("DROP TABLE bill_table");

    run_ddl_statement("DROP USER foo;");
    run_ddl_statement("DROP ROLE salesDept;");
    run_ddl_statement("DROP USER bob;");
  }
  explicit ViewObject() { setup_objects(); }
  virtual ~ViewObject() { remove_objects(); }
};

struct DashboardObject : testing::Test {
  const std::string dname1 = "ChampionsLeague";
  const std::string dname2 = "Europa";
  const std::string dstate = "active";
  const std::string dhash = "image00";
  const std::string dmeta = "Chelsea are champions";
  int id;
  Users user_;
  Roles role_;

  FrontendViewDescriptor vd1;
  void setup_dashboards() {
    auto& gcat = g_session->get_catalog();
    vd1.viewName = dname1;
    vd1.viewState = dstate;
    vd1.imageHash = dhash;
    vd1.viewMetadata = dmeta;
    vd1.userId = g_session->get_currentUser().userId;
    vd1.user = g_session->get_currentUser().userName;
    id = gcat.createFrontendView(vd1);
    sys_cat.createDBObject(g_session->get_currentUser(), dname1, DBObjectType::DashboardDBObjectType, gcat, id);
  }

  void drop_dashboards() {
    auto& gcat = g_session->get_catalog();
    if (gcat.getMetadataForDashboard(id)) {
      gcat.deleteMetadataForDashboard(id);
    }
  }
  explicit DashboardObject() {
    drop_dashboards();
    setup_dashboards();
  }
  virtual ~DashboardObject() { drop_dashboards(); }
};

TEST_F(DatabaseObject, AccessDefaultsTest) {
  auto cat_mapd = Catalog_Namespace::Catalog::get("mapd");
  DBObject mapd_object("mapd", DBObjectType::DatabaseDBObjectType);
  privObjects.clear();
  mapd_object.loadKey(*cat_mapd);

  privObjects.push_back(mapd_object);
  EXPECT_EQ(sys_cat.checkPrivileges("Chelsea", privObjects), true);
  EXPECT_EQ(sys_cat.checkPrivileges("Arsenal", privObjects), false);
  EXPECT_EQ(sys_cat.checkPrivileges("Juventus", privObjects), false);
  EXPECT_EQ(sys_cat.checkPrivileges("Bayern", privObjects), false);
}

TEST_F(DatabaseObject, TableAccessTest) {
  std::unique_ptr<Catalog_Namespace::SessionInfo> session_ars;
  boost::filesystem::path base_path{BASE_PATH};
  auto system_db_file = base_path / "mapd_catalogs" / "mapd";
  auto data_dir = base_path / "mapd_data";
  auto dataMgr = std::make_shared<Data_Namespace::DataMgr>(data_dir.string(), 0, false, 0);
  sys_cat.getMetadataForDB("mapd", db_meta);
  sys_cat.getMetadataForUser("Arsenal", user_meta);
  session_ars.reset(
      new Catalog_Namespace::SessionInfo(std::make_shared<Catalog_Namespace::Catalog>(
                                             base_path.string(), db, dataMgr, std::vector<LeafHostInfo>{}, g_calcite),
                                         user_meta,
                                         ExecutorDeviceType::GPU,
                                         ""));
  auto& cat_mapd = session_ars->get_catalog();
  AccessPrivileges arsenal_privs;
  ASSERT_NO_THROW(arsenal_privs.add(AccessPrivileges::CREATE_TABLE));
  ASSERT_NO_THROW(arsenal_privs.add(AccessPrivileges::DROP_TABLE));
  DBObject mapd_object("mapd", DBObjectType::DatabaseDBObjectType);
  privObjects.clear();
  mapd_object.loadKey(cat_mapd);
  mapd_object.resetPrivileges();
  ASSERT_NO_THROW(mapd_object.setPrivileges(arsenal_privs));
  ASSERT_NO_THROW(sys_cat.grantDBObjectPrivileges("Arsenal", mapd_object, cat_mapd));

  mapd_object.resetPrivileges();
  ASSERT_NO_THROW(mapd_object.setPrivileges(arsenal_privs));
  privObjects.push_back(mapd_object);
  EXPECT_EQ(sys_cat.checkPrivileges("Arsenal", privObjects), true);

  mapd_object.resetPrivileges();
  privObjects.clear();
  ASSERT_NO_THROW(arsenal_privs.remove(AccessPrivileges::CREATE_TABLE));
  ASSERT_NO_THROW(mapd_object.setPrivileges(arsenal_privs));
  ASSERT_NO_THROW(sys_cat.revokeDBObjectPrivileges("Arsenal", mapd_object, cat_mapd));

  mapd_object.resetPrivileges();
  privObjects.clear();
  ASSERT_NO_THROW(mapd_object.setPrivileges(AccessPrivileges::CREATE_TABLE));
  privObjects.push_back(mapd_object);
  EXPECT_EQ(sys_cat.checkPrivileges("Arsenal", privObjects), true);
}

TEST_F(DatabaseObject, ViewAccessTest) {
  std::unique_ptr<Catalog_Namespace::SessionInfo> session_ars;
  boost::filesystem::path base_path{BASE_PATH};
  auto system_db_file = base_path / "mapd_catalogs" / "mapd";
  auto data_dir = base_path / "mapd_data";
  auto dataMgr = std::make_shared<Data_Namespace::DataMgr>(data_dir.string(), 0, false, 0);
  sys_cat.getMetadataForDB("mapd", db_meta);
  sys_cat.getMetadataForUser("Arsenal", user_meta);
  session_ars.reset(
      new Catalog_Namespace::SessionInfo(std::make_shared<Catalog_Namespace::Catalog>(
                                             base_path.string(), db, dataMgr, std::vector<LeafHostInfo>{}, g_calcite),
                                         user_meta,
                                         ExecutorDeviceType::GPU,
                                         ""));
  auto& cat_mapd = session_ars->get_catalog();
  AccessPrivileges arsenal_privs;
  ASSERT_NO_THROW(arsenal_privs.add(AccessPrivileges::ALL_VIEW));
  DBObject mapd_object("mapd", DBObjectType::DatabaseDBObjectType);
  privObjects.clear();
  mapd_object.loadKey(cat_mapd);
  mapd_object.resetPrivileges();
  ASSERT_NO_THROW(mapd_object.setPrivileges(arsenal_privs));
  ASSERT_NO_THROW(sys_cat.grantDBObjectPrivileges("Arsenal", mapd_object, cat_mapd));

  mapd_object.resetPrivileges();
  ASSERT_NO_THROW(mapd_object.setPrivileges(arsenal_privs));
  privObjects.push_back(mapd_object);
  EXPECT_EQ(sys_cat.checkPrivileges("Arsenal", privObjects), true);

  mapd_object.resetPrivileges();
  privObjects.clear();
  ASSERT_NO_THROW(arsenal_privs.remove(AccessPrivileges::DROP_VIEW));
  ASSERT_NO_THROW(arsenal_privs.remove(AccessPrivileges::TRUNCATE_VIEW));
  ASSERT_NO_THROW(mapd_object.setPrivileges(arsenal_privs));
  ASSERT_NO_THROW(sys_cat.revokeDBObjectPrivileges("Arsenal", mapd_object, cat_mapd));

  mapd_object.resetPrivileges();
  privObjects.clear();
  ASSERT_NO_THROW(mapd_object.setPrivileges(AccessPrivileges::ALL_VIEW));
  privObjects.push_back(mapd_object);
  EXPECT_EQ(sys_cat.checkPrivileges("Arsenal", privObjects), false);
  mapd_object.resetPrivileges();
  privObjects.clear();
  ASSERT_NO_THROW(mapd_object.setPrivileges(AccessPrivileges::DROP_VIEW));
  privObjects.push_back(mapd_object);
  EXPECT_EQ(sys_cat.checkPrivileges("Arsenal", privObjects), true);
  mapd_object.resetPrivileges();
  privObjects.clear();
  ASSERT_NO_THROW(mapd_object.setPrivileges(AccessPrivileges::TRUNCATE_VIEW));
  privObjects.push_back(mapd_object);
  EXPECT_EQ(sys_cat.checkPrivileges("Arsenal", privObjects), true);
}

TEST_F(DatabaseObject, DashboardAccessTest) {
  std::unique_ptr<Catalog_Namespace::SessionInfo> session_ars;
  boost::filesystem::path base_path{BASE_PATH};
  auto system_db_file = base_path / "mapd_catalogs" / "mapd";
  auto data_dir = base_path / "mapd_data";
  auto dataMgr = std::make_shared<Data_Namespace::DataMgr>(data_dir.string(), 0, false, 0);
  sys_cat.getMetadataForDB("mapd", db_meta);
  sys_cat.getMetadataForUser("Arsenal", user_meta);
  session_ars.reset(
      new Catalog_Namespace::SessionInfo(std::make_shared<Catalog_Namespace::Catalog>(
                                             base_path.string(), db, dataMgr, std::vector<LeafHostInfo>{}, g_calcite),
                                         user_meta,
                                         ExecutorDeviceType::GPU,
                                         ""));
  auto& cat_mapd = session_ars->get_catalog();
  AccessPrivileges arsenal_privs;
  ASSERT_NO_THROW(arsenal_privs.add(AccessPrivileges::ALL_DASHBOARD));
  DBObject mapd_object("mapd", DBObjectType::DatabaseDBObjectType);
  privObjects.clear();
  mapd_object.loadKey(cat_mapd);
  mapd_object.resetPrivileges();
  ASSERT_NO_THROW(mapd_object.setPrivileges(arsenal_privs));
  ASSERT_NO_THROW(sys_cat.grantDBObjectPrivileges("Arsenal", mapd_object, cat_mapd));

  mapd_object.resetPrivileges();
  ASSERT_NO_THROW(mapd_object.setPrivileges(arsenal_privs));
  privObjects.push_back(mapd_object);
  EXPECT_EQ(sys_cat.checkPrivileges("Arsenal", privObjects), true);

  mapd_object.resetPrivileges();
  privObjects.clear();
  ASSERT_NO_THROW(arsenal_privs.remove(AccessPrivileges::EDIT_DASHBOARD));
  ASSERT_NO_THROW(mapd_object.setPrivileges(arsenal_privs));
  ASSERT_NO_THROW(sys_cat.revokeDBObjectPrivileges("Arsenal", mapd_object, cat_mapd));

  mapd_object.resetPrivileges();
  privObjects.clear();
  ASSERT_NO_THROW(mapd_object.setPrivileges(AccessPrivileges::CREATE_DASHBOARD));
  privObjects.push_back(mapd_object);
  EXPECT_EQ(sys_cat.checkPrivileges("Arsenal", privObjects), false);
  mapd_object.resetPrivileges();
  privObjects.clear();
  ASSERT_NO_THROW(mapd_object.setPrivileges(AccessPrivileges::DELETE_DASHBOARD));
  privObjects.push_back(mapd_object);
  EXPECT_EQ(sys_cat.checkPrivileges("Arsenal", privObjects), false);
  mapd_object.resetPrivileges();
  privObjects.clear();
  ASSERT_NO_THROW(mapd_object.setPrivileges(AccessPrivileges::EDIT_DASHBOARD));
  privObjects.push_back(mapd_object);
  EXPECT_EQ(sys_cat.checkPrivileges("Arsenal", privObjects), true);
}

TEST_F(TableObject, AccessDefaultsTest) {
  auto& g_cat = g_session->get_catalog();
  ASSERT_NO_THROW(sys_cat.grantRole("Sudens", "Bayern"));
  ASSERT_NO_THROW(sys_cat.grantRole("OldLady", "Juventus"));
  AccessPrivileges epl_privs;
  AccessPrivileges seriea_privs;
  AccessPrivileges bundesliga_privs;
  ASSERT_NO_THROW(epl_privs.add(AccessPrivileges::SELECT_FROM_TABLE));
  ASSERT_NO_THROW(seriea_privs.add(AccessPrivileges::SELECT_FROM_TABLE));
  ASSERT_NO_THROW(bundesliga_privs.add(AccessPrivileges::SELECT_FROM_TABLE));
  privObjects.clear();
  DBObject epl_object("epl", DBObjectType::TableDBObjectType);
  DBObject seriea_object("seriea", DBObjectType::TableDBObjectType);
  DBObject bundesliga_object("bundesliga", DBObjectType::TableDBObjectType);
  epl_object.loadKey(g_cat);
  seriea_object.loadKey(g_cat);
  bundesliga_object.loadKey(g_cat);
  ASSERT_NO_THROW(epl_object.setPrivileges(epl_privs));
  ASSERT_NO_THROW(seriea_object.setPrivileges(seriea_privs));
  ASSERT_NO_THROW(bundesliga_object.setPrivileges(bundesliga_privs));
  privObjects.push_back(epl_object);
  privObjects.push_back(seriea_object);
  privObjects.push_back(bundesliga_object);

  EXPECT_EQ(sys_cat.checkPrivileges("Chelsea", privObjects), true);
  EXPECT_EQ(sys_cat.checkPrivileges("Arsenal", privObjects), false);
  EXPECT_EQ(sys_cat.checkPrivileges("Juventus", privObjects), false);
  EXPECT_EQ(sys_cat.checkPrivileges("Bayern", privObjects), false);
}
TEST_F(TableObject, AccessAfterGrantsTest) {
  auto& g_cat = g_session->get_catalog();
  ASSERT_NO_THROW(sys_cat.grantRole("Sudens", "Bayern"));
  ASSERT_NO_THROW(sys_cat.grantRole("OldLady", "Juventus"));
  AccessPrivileges epl_privs;
  AccessPrivileges seriea_privs;
  AccessPrivileges bundesliga_privs;
  ASSERT_NO_THROW(epl_privs.add(AccessPrivileges::SELECT_FROM_TABLE));
  ASSERT_NO_THROW(seriea_privs.add(AccessPrivileges::SELECT_FROM_TABLE));
  ASSERT_NO_THROW(bundesliga_privs.add(AccessPrivileges::SELECT_FROM_TABLE));
  privObjects.clear();
  DBObject epl_object("epl", DBObjectType::TableDBObjectType);
  DBObject seriea_object("seriea", DBObjectType::TableDBObjectType);
  DBObject bundesliga_object("bundesliga", DBObjectType::TableDBObjectType);
  epl_object.loadKey(g_cat);
  seriea_object.loadKey(g_cat);
  bundesliga_object.loadKey(g_cat);
  ASSERT_NO_THROW(epl_object.setPrivileges(epl_privs));
  ASSERT_NO_THROW(seriea_object.setPrivileges(seriea_privs));
  ASSERT_NO_THROW(bundesliga_object.setPrivileges(bundesliga_privs));
  ASSERT_NO_THROW(sys_cat.grantDBObjectPrivileges("Arsenal", epl_object, g_cat));
  ASSERT_NO_THROW(sys_cat.grantDBObjectPrivileges("Sudens", bundesliga_object, g_cat));
  ASSERT_NO_THROW(sys_cat.grantDBObjectPrivileges("OldLady", seriea_object, g_cat));

  privObjects.push_back(epl_object);
  EXPECT_EQ(sys_cat.checkPrivileges("Chelsea", privObjects), true);
  EXPECT_EQ(sys_cat.checkPrivileges("Arsenal", privObjects), true);
  EXPECT_EQ(sys_cat.checkPrivileges("Juventus", privObjects), false);
  EXPECT_EQ(sys_cat.checkPrivileges("Bayern", privObjects), false);
  privObjects.clear();
  privObjects.push_back(seriea_object);
  EXPECT_EQ(sys_cat.checkPrivileges("Chelsea", privObjects), true);
  EXPECT_EQ(sys_cat.checkPrivileges("Arsenal", privObjects), false);
  EXPECT_EQ(sys_cat.checkPrivileges("Juventus", privObjects), true);
  EXPECT_EQ(sys_cat.checkPrivileges("Bayern", privObjects), false);
  privObjects.clear();
  privObjects.push_back(bundesliga_object);
  EXPECT_EQ(sys_cat.checkPrivileges("Chelsea", privObjects), true);
  EXPECT_EQ(sys_cat.checkPrivileges("Arsenal", privObjects), false);
  EXPECT_EQ(sys_cat.checkPrivileges("Juventus", privObjects), false);
  EXPECT_EQ(sys_cat.checkPrivileges("Bayern", privObjects), true);
}

TEST_F(TableObject, AccessAfterRevokesTest) {
  auto& g_cat = g_session->get_catalog();
  ASSERT_NO_THROW(sys_cat.grantRole("OldLady", "Juventus"));
  ASSERT_NO_THROW(sys_cat.grantRole("Gunners", "Arsenal"));
  AccessPrivileges epl_privs;
  AccessPrivileges seriea_privs;
  AccessPrivileges bundesliga_privs;
  ASSERT_NO_THROW(epl_privs.add(AccessPrivileges::SELECT_FROM_TABLE));
  ASSERT_NO_THROW(epl_privs.add(AccessPrivileges::INSERT_INTO_TABLE));
  ASSERT_NO_THROW(seriea_privs.add(AccessPrivileges::SELECT_FROM_TABLE));
  ASSERT_NO_THROW(bundesliga_privs.add(AccessPrivileges::ALL_TABLE));
  privObjects.clear();
  DBObject epl_object("epl", DBObjectType::TableDBObjectType);
  DBObject seriea_object("seriea", DBObjectType::TableDBObjectType);
  DBObject bundesliga_object("bundesliga", DBObjectType::TableDBObjectType);
  epl_object.loadKey(g_cat);
  seriea_object.loadKey(g_cat);
  bundesliga_object.loadKey(g_cat);
  ASSERT_NO_THROW(epl_object.setPrivileges(epl_privs));
  ASSERT_NO_THROW(seriea_object.setPrivileges(seriea_privs));
  ASSERT_NO_THROW(bundesliga_object.setPrivileges(bundesliga_privs));
  ASSERT_NO_THROW(sys_cat.grantDBObjectPrivileges("Gunners", epl_object, g_cat));
  ASSERT_NO_THROW(sys_cat.grantDBObjectPrivileges("Bayern", bundesliga_object, g_cat));
  ASSERT_NO_THROW(sys_cat.grantDBObjectPrivileges("OldLady", seriea_object, g_cat));

  privObjects.push_back(epl_object);
  EXPECT_EQ(sys_cat.checkPrivileges("Chelsea", privObjects), true);
  EXPECT_EQ(sys_cat.checkPrivileges("Arsenal", privObjects), true);
  EXPECT_EQ(sys_cat.checkPrivileges("Juventus", privObjects), false);
  EXPECT_EQ(sys_cat.checkPrivileges("Bayern", privObjects), false);
  privObjects.clear();
  privObjects.push_back(seriea_object);
  EXPECT_EQ(sys_cat.checkPrivileges("Chelsea", privObjects), true);
  EXPECT_EQ(sys_cat.checkPrivileges("Arsenal", privObjects), false);
  EXPECT_EQ(sys_cat.checkPrivileges("Juventus", privObjects), true);
  EXPECT_EQ(sys_cat.checkPrivileges("Bayern", privObjects), false);
  privObjects.clear();
  privObjects.push_back(bundesliga_object);
  EXPECT_EQ(sys_cat.checkPrivileges("Chelsea", privObjects), true);
  EXPECT_EQ(sys_cat.checkPrivileges("Arsenal", privObjects), false);
  EXPECT_EQ(sys_cat.checkPrivileges("Juventus", privObjects), false);
  EXPECT_EQ(sys_cat.checkPrivileges("Bayern", privObjects), true);

  epl_object.resetPrivileges();
  seriea_object.resetPrivileges();
  bundesliga_object.resetPrivileges();
  ASSERT_NO_THROW(epl_privs.remove(AccessPrivileges::SELECT_FROM_TABLE));
  ASSERT_NO_THROW(epl_object.setPrivileges(epl_privs));
  ASSERT_NO_THROW(seriea_object.setPrivileges(seriea_privs));
  ASSERT_NO_THROW(bundesliga_object.setPrivileges(bundesliga_privs));
  ASSERT_NO_THROW(sys_cat.revokeDBObjectPrivileges("Gunners", epl_object, g_cat));
  ASSERT_NO_THROW(sys_cat.revokeDBObjectPrivileges("Bayern", bundesliga_object, g_cat));
  ASSERT_NO_THROW(sys_cat.revokeDBObjectPrivileges("OldLady", seriea_object, g_cat));

  epl_object.resetPrivileges();

  ASSERT_NO_THROW(epl_object.setPrivileges(AccessPrivileges::SELECT_FROM_TABLE));
  privObjects.clear();
  privObjects.push_back(epl_object);
  EXPECT_EQ(sys_cat.checkPrivileges("Chelsea", privObjects), true);
  EXPECT_EQ(sys_cat.checkPrivileges("Arsenal", privObjects), true);

  epl_object.resetPrivileges();
  ASSERT_NO_THROW(epl_object.setPrivileges(AccessPrivileges::INSERT_INTO_TABLE));
  privObjects.clear();
  privObjects.push_back(epl_object);
  EXPECT_EQ(sys_cat.checkPrivileges("Chelsea", privObjects), true);
  EXPECT_EQ(sys_cat.checkPrivileges("Arsenal", privObjects), false);

  seriea_object.resetPrivileges();
  ASSERT_NO_THROW(seriea_object.setPrivileges(AccessPrivileges::SELECT_FROM_TABLE));
  privObjects.clear();
  privObjects.push_back(seriea_object);
  EXPECT_EQ(sys_cat.checkPrivileges("Chelsea", privObjects), true);
  EXPECT_EQ(sys_cat.checkPrivileges("Juventus", privObjects), false);

  seriea_object.resetPrivileges();
  ASSERT_NO_THROW(seriea_object.setPrivileges(AccessPrivileges::INSERT_INTO_TABLE));
  privObjects.clear();
  privObjects.push_back(seriea_object);
  EXPECT_EQ(sys_cat.checkPrivileges("Chelsea", privObjects), true);
  EXPECT_EQ(sys_cat.checkPrivileges("Juventus", privObjects), false);

  bundesliga_object.resetPrivileges();
  ASSERT_NO_THROW(bundesliga_object.setPrivileges(AccessPrivileges::ALL_TABLE));
  privObjects.clear();
  privObjects.push_back(bundesliga_object);
  EXPECT_EQ(sys_cat.checkPrivileges("Chelsea", privObjects), true);
  EXPECT_EQ(sys_cat.checkPrivileges("Bayern", privObjects), false);
}

void testViewPermissions(std::string user, std::string roleToGrant) {
  DBObject bill_view("bill_view", DBObjectType::ViewDBObjectType);
  auto& cat = g_session->get_catalog();
  bill_view.loadKey(cat);
  std::vector<DBObject> privs;

  bill_view.setPrivileges(AccessPrivileges::CREATE_VIEW);
  privs = {bill_view};
  EXPECT_EQ(sys_cat.checkPrivileges(user, privs), false);

  bill_view.setPrivileges(AccessPrivileges::DROP_VIEW);
  privs = {bill_view};
  EXPECT_EQ(sys_cat.checkPrivileges(user, privs), false);

  bill_view.setPrivileges(AccessPrivileges::SELECT_FROM_VIEW);
  privs = {bill_view};
  EXPECT_EQ(sys_cat.checkPrivileges(user, privs), false);

  bill_view.setPrivileges(AccessPrivileges::CREATE_VIEW);
  sys_cat.grantDBObjectPrivileges(roleToGrant, bill_view, cat);
  bill_view.setPrivileges(AccessPrivileges::CREATE_VIEW);
  privs = {bill_view};
  EXPECT_EQ(sys_cat.checkPrivileges(user, privs), true);

  bill_view.setPrivileges(AccessPrivileges::DROP_VIEW);
  sys_cat.grantDBObjectPrivileges(roleToGrant, bill_view, cat);
  bill_view.setPrivileges(AccessPrivileges::DROP_VIEW);
  privs = {bill_view};
  EXPECT_EQ(sys_cat.checkPrivileges(user, privs), true);

  bill_view.setPrivileges(AccessPrivileges::SELECT_FROM_VIEW);
  sys_cat.grantDBObjectPrivileges(roleToGrant, bill_view, cat);
  bill_view.setPrivileges(AccessPrivileges::SELECT_FROM_VIEW);
  privs = {bill_view};
  EXPECT_EQ(sys_cat.checkPrivileges(user, privs), true);

  bill_view.setPrivileges(AccessPrivileges::CREATE_VIEW);
  sys_cat.revokeDBObjectPrivileges(roleToGrant, bill_view, cat);
  bill_view.setPrivileges(AccessPrivileges::CREATE_VIEW);
  privs = {bill_view};
  EXPECT_EQ(sys_cat.checkPrivileges(user, privs), false);

  bill_view.setPrivileges(AccessPrivileges::DROP_VIEW);
  sys_cat.revokeDBObjectPrivileges(roleToGrant, bill_view, cat);
  bill_view.setPrivileges(AccessPrivileges::DROP_VIEW);
  privs = {bill_view};
  EXPECT_EQ(sys_cat.checkPrivileges(user, privs), false);

  bill_view.setPrivileges(AccessPrivileges::SELECT_FROM_VIEW);
  sys_cat.revokeDBObjectPrivileges(roleToGrant, bill_view, cat);
  bill_view.setPrivileges(AccessPrivileges::SELECT_FROM_VIEW);
  privs = {bill_view};
  EXPECT_EQ(sys_cat.checkPrivileges(user, privs), false);
}

TEST_F(ViewObject, UserRoleBobGetsGrants) {
  testViewPermissions("bob", "bob");
}

TEST_F(ViewObject, GroupRoleFooGetsGrants) {
  testViewPermissions("foo", "salesDept");
}

TEST_F(ViewObject, CalciteViewResolution) {
  TPlanResult result = ::g_calcite->process(*g_session, "select * from bill_table", true, false);
  EXPECT_EQ(result.primary_accessed_objects.tables_selected_from.size(), (size_t)1);
  EXPECT_EQ(result.primary_accessed_objects.tables_inserted_into.size(), (size_t)0);
  EXPECT_EQ(result.primary_accessed_objects.tables_updated_in.size(), (size_t)0);
  EXPECT_EQ(result.primary_accessed_objects.tables_deleted_from.size(), (size_t)0);
  EXPECT_EQ(result.primary_accessed_objects.tables_selected_from[0], "bill_table");
  EXPECT_EQ(result.resolved_accessed_objects.tables_selected_from.size(), (size_t)1);
  EXPECT_EQ(result.resolved_accessed_objects.tables_inserted_into.size(), (size_t)0);
  EXPECT_EQ(result.resolved_accessed_objects.tables_updated_in.size(), (size_t)0);
  EXPECT_EQ(result.resolved_accessed_objects.tables_deleted_from.size(), (size_t)0);
  EXPECT_EQ(result.resolved_accessed_objects.tables_selected_from[0], "bill_table");

  result = ::g_calcite->process(*g_session, "select * from bill_view", true, false);
  EXPECT_EQ(result.primary_accessed_objects.tables_selected_from.size(), (size_t)1);
  EXPECT_EQ(result.primary_accessed_objects.tables_inserted_into.size(), (size_t)0);
  EXPECT_EQ(result.primary_accessed_objects.tables_updated_in.size(), (size_t)0);
  EXPECT_EQ(result.primary_accessed_objects.tables_deleted_from.size(), (size_t)0);
  EXPECT_EQ(result.primary_accessed_objects.tables_selected_from[0], "bill_view");
  EXPECT_EQ(result.resolved_accessed_objects.tables_selected_from.size(), (size_t)1);
  EXPECT_EQ(result.resolved_accessed_objects.tables_inserted_into.size(), (size_t)0);
  EXPECT_EQ(result.resolved_accessed_objects.tables_updated_in.size(), (size_t)0);
  EXPECT_EQ(result.resolved_accessed_objects.tables_deleted_from.size(), (size_t)0);
  EXPECT_EQ(result.resolved_accessed_objects.tables_selected_from[0], "bill_table");

  result = ::g_calcite->process(*g_session, "select * from bill_view_outer", true, false);
  EXPECT_EQ(result.primary_accessed_objects.tables_selected_from.size(), (size_t)1);
  EXPECT_EQ(result.primary_accessed_objects.tables_inserted_into.size(), (size_t)0);
  EXPECT_EQ(result.primary_accessed_objects.tables_updated_in.size(), (size_t)0);
  EXPECT_EQ(result.primary_accessed_objects.tables_deleted_from.size(), (size_t)0);
  EXPECT_EQ(result.primary_accessed_objects.tables_selected_from[0], "bill_view_outer");
  EXPECT_EQ(result.resolved_accessed_objects.tables_selected_from.size(), (size_t)1);
  EXPECT_EQ(result.resolved_accessed_objects.tables_inserted_into.size(), (size_t)0);
  EXPECT_EQ(result.resolved_accessed_objects.tables_updated_in.size(), (size_t)0);
  EXPECT_EQ(result.resolved_accessed_objects.tables_deleted_from.size(), (size_t)0);
  EXPECT_EQ(result.resolved_accessed_objects.tables_selected_from[0], "bill_table");
}

TEST_F(DashboardObject, AccessDefaultsTest) {
  auto& g_cat = g_session->get_catalog();
  ASSERT_NO_THROW(sys_cat.grantRole("Gunners", "Bayern"));
  ASSERT_NO_THROW(sys_cat.grantRole("Sudens", "Arsenal"));
  AccessPrivileges dash_priv;
  ASSERT_NO_THROW(dash_priv.add(AccessPrivileges::VIEW_DASHBOARD));
  privObjects.clear();
  DBObject dash_object(id, DBObjectType::DashboardDBObjectType);
  dash_object.loadKey(g_cat);
  ASSERT_NO_THROW(dash_object.setPrivileges(dash_priv));
  privObjects.push_back(dash_object);

  EXPECT_EQ(sys_cat.checkPrivileges("Chelsea", privObjects), true);
  EXPECT_EQ(sys_cat.checkPrivileges("Arsenal", privObjects), false);
  EXPECT_EQ(sys_cat.checkPrivileges("Juventus", privObjects), false);
  EXPECT_EQ(sys_cat.checkPrivileges("Bayern", privObjects), false);
}

TEST_F(DashboardObject, AccessAfterGrantsTest) {
  auto& g_cat = g_session->get_catalog();
  ASSERT_NO_THROW(sys_cat.grantRole("Gunners", "Arsenal"));
  AccessPrivileges dash_priv;
  ASSERT_NO_THROW(dash_priv.add(AccessPrivileges::VIEW_DASHBOARD));
  privObjects.clear();
  DBObject dash_object(id, DBObjectType::DashboardDBObjectType);
  dash_object.loadKey(g_cat);
  ASSERT_NO_THROW(dash_object.setPrivileges(dash_priv));
  privObjects.push_back(dash_object);
  ASSERT_NO_THROW(sys_cat.grantDBObjectPrivileges("Gunners", dash_object, g_cat));
  ASSERT_NO_THROW(sys_cat.grantDBObjectPrivileges("Juventus", dash_object, g_cat));

  privObjects.clear();
  privObjects.push_back(dash_object);
  EXPECT_EQ(sys_cat.checkPrivileges("Chelsea", privObjects), true);
  EXPECT_EQ(sys_cat.checkPrivileges("Arsenal", privObjects), true);
  EXPECT_EQ(sys_cat.checkPrivileges("Juventus", privObjects), true);
  EXPECT_EQ(sys_cat.checkPrivileges("Bayern", privObjects), false);
}

TEST_F(DashboardObject, AccessAfterRevokesTest) {
  auto& g_cat = g_session->get_catalog();
  ASSERT_NO_THROW(sys_cat.grantRole("OldLady", "Juventus"));
  ASSERT_NO_THROW(sys_cat.grantRole("Sudens", "Bayern"));
  AccessPrivileges dash_priv;
  ASSERT_NO_THROW(dash_priv.add(AccessPrivileges::VIEW_DASHBOARD));
  privObjects.clear();
  DBObject dash_object(id, DBObjectType::DashboardDBObjectType);
  dash_object.loadKey(g_cat);
  ASSERT_NO_THROW(dash_object.setPrivileges(dash_priv));
  privObjects.push_back(dash_object);
  ASSERT_NO_THROW(sys_cat.grantDBObjectPrivileges("OldLady", dash_object, g_cat));
  ASSERT_NO_THROW(sys_cat.grantDBObjectPrivileges("Arsenal", dash_object, g_cat));

  EXPECT_EQ(sys_cat.checkPrivileges("Chelsea", privObjects), true);
  EXPECT_EQ(sys_cat.checkPrivileges("Arsenal", privObjects), true);
  EXPECT_EQ(sys_cat.checkPrivileges("Juventus", privObjects), true);
  EXPECT_EQ(sys_cat.checkPrivileges("Bayern", privObjects), false);

  ASSERT_NO_THROW(sys_cat.revokeDBObjectPrivileges("OldLady", dash_object, g_cat));
  EXPECT_EQ(sys_cat.checkPrivileges("Chelsea", privObjects), true);
  EXPECT_EQ(sys_cat.checkPrivileges("Arsenal", privObjects), true);
  EXPECT_EQ(sys_cat.checkPrivileges("Juventus", privObjects), false);
  EXPECT_EQ(sys_cat.checkPrivileges("Bayern", privObjects), false);

  ASSERT_NO_THROW(sys_cat.revokeDBObjectPrivileges("Arsenal", dash_object, g_cat));
  ASSERT_NO_THROW(sys_cat.grantDBObjectPrivileges("Bayern", dash_object, g_cat));
  EXPECT_EQ(sys_cat.checkPrivileges("Chelsea", privObjects), true);
  EXPECT_EQ(sys_cat.checkPrivileges("Arsenal", privObjects), false);
  EXPECT_EQ(sys_cat.checkPrivileges("Juventus", privObjects), false);
  EXPECT_EQ(sys_cat.checkPrivileges("Bayern", privObjects), true);
}

TEST_F(DashboardObject, GranteesDefaultListTest) {
  auto& g_cat = g_session->get_catalog();
  auto perms_list = sys_cat.getMetadataForObject(
      g_cat.get_currentDB().dbId, static_cast<int>(DBObjectType::DashboardDBObjectType), id);
  int size = static_cast<int>(perms_list.size());
  ASSERT_EQ(size, 0);
}

TEST_F(DashboardObject, GranteesListAfterGrantsTest) {
  auto& g_cat = g_session->get_catalog();
  auto perms_list = sys_cat.getMetadataForObject(
      g_cat.get_currentDB().dbId, static_cast<int>(DBObjectType::DashboardDBObjectType), id);
  int recs1 = static_cast<int>(perms_list.size());
  ASSERT_NO_THROW(sys_cat.grantRole("OldLady", "Juventus"));
  AccessPrivileges dash_priv;
  ASSERT_NO_THROW(dash_priv.add(AccessPrivileges::VIEW_DASHBOARD));
  privObjects.clear();
  DBObject dash_object(id, DBObjectType::DashboardDBObjectType);
  dash_object.loadKey(g_cat);
  ASSERT_NO_THROW(dash_object.setPrivileges(dash_priv));
  privObjects.push_back(dash_object);
  ASSERT_NO_THROW(sys_cat.grantDBObjectPrivileges("OldLady", dash_object, g_cat));
  ASSERT_NO_THROW(sys_cat.grantDBObjectPrivileges("Bayern", dash_object, g_cat));
  perms_list = sys_cat.getMetadataForObject(
      g_cat.get_currentDB().dbId, static_cast<int>(DBObjectType::DashboardDBObjectType), id);
  int recs2 = static_cast<int>(perms_list.size());
  ASSERT_NE(recs1, recs2);
  ASSERT_EQ(recs2, 2);
  ASSERT_TRUE(perms_list[0]->privs.hasPermission(DashboardPrivileges::VIEW_DASHBOARD));
  ASSERT_TRUE(perms_list[1]->privs.hasPermission(DashboardPrivileges::VIEW_DASHBOARD));
  ASSERT_FALSE(perms_list[1]->privs.hasPermission(DashboardPrivileges::EDIT_DASHBOARD));

  ASSERT_NO_THROW(dash_priv.add(AccessPrivileges::EDIT_DASHBOARD));
  ASSERT_NO_THROW(dash_object.setPrivileges(dash_priv));
  ASSERT_NO_THROW(sys_cat.grantDBObjectPrivileges("Bayern", dash_object, g_cat));
  perms_list = sys_cat.getMetadataForObject(
      g_cat.get_currentDB().dbId, static_cast<int>(DBObjectType::DashboardDBObjectType), id);
  int recs3 = static_cast<int>(perms_list.size());
  ASSERT_EQ(recs3, 2);
  ASSERT_TRUE(perms_list[0]->privs.hasPermission(DashboardPrivileges::VIEW_DASHBOARD));
  ASSERT_TRUE(perms_list[1]->privs.hasPermission(DashboardPrivileges::VIEW_DASHBOARD));
  ASSERT_TRUE(perms_list[1]->privs.hasPermission(DashboardPrivileges::EDIT_DASHBOARD));
}

TEST_F(DashboardObject, GranteesListAfterRevokesTest) {
  auto& g_cat = g_session->get_catalog();
  auto perms_list = sys_cat.getMetadataForObject(
      g_cat.get_currentDB().dbId, static_cast<int>(DBObjectType::DashboardDBObjectType), id);
  int recs1 = static_cast<int>(perms_list.size());
  ASSERT_NO_THROW(sys_cat.grantRole("Gunners", "Arsenal"));
  AccessPrivileges dash_priv;
  ASSERT_NO_THROW(dash_priv.add(AccessPrivileges::VIEW_DASHBOARD));
  ASSERT_NO_THROW(dash_priv.add(AccessPrivileges::EDIT_DASHBOARD));
  privObjects.clear();
  DBObject dash_object(id, DBObjectType::DashboardDBObjectType);
  dash_object.loadKey(g_cat);
  ASSERT_NO_THROW(dash_object.setPrivileges(dash_priv));
  privObjects.push_back(dash_object);
  ASSERT_NO_THROW(sys_cat.grantDBObjectPrivileges("Gunners", dash_object, g_cat));
  ASSERT_NO_THROW(sys_cat.grantDBObjectPrivileges("Chelsea", dash_object, g_cat));
  perms_list = sys_cat.getMetadataForObject(
      g_cat.get_currentDB().dbId, static_cast<int>(DBObjectType::DashboardDBObjectType), id);
  int recs2 = static_cast<int>(perms_list.size());
  ASSERT_NE(recs1, recs2);
  ASSERT_EQ(recs2, 2);
  ASSERT_TRUE(perms_list[0]->privs.hasPermission(DashboardPrivileges::VIEW_DASHBOARD));
  ASSERT_TRUE(perms_list[0]->privs.hasPermission(DashboardPrivileges::EDIT_DASHBOARD));
  ASSERT_TRUE(perms_list[1]->privs.hasPermission(DashboardPrivileges::VIEW_DASHBOARD));
  ASSERT_TRUE(perms_list[1]->privs.hasPermission(DashboardPrivileges::EDIT_DASHBOARD));

  ASSERT_NO_THROW(dash_priv.remove(AccessPrivileges::VIEW_DASHBOARD));
  ASSERT_NO_THROW(dash_object.setPrivileges(dash_priv));
  ASSERT_NO_THROW(sys_cat.revokeDBObjectPrivileges("Gunners", dash_object, g_cat));
  perms_list = sys_cat.getMetadataForObject(
      g_cat.get_currentDB().dbId, static_cast<int>(DBObjectType::DashboardDBObjectType), id);
  int recs3 = static_cast<int>(perms_list.size());
  ASSERT_EQ(recs3, 2);
  ASSERT_TRUE(perms_list[0]->privs.hasPermission(DashboardPrivileges::VIEW_DASHBOARD));
  ASSERT_FALSE(perms_list[0]->privs.hasPermission(DashboardPrivileges::EDIT_DASHBOARD));
  ASSERT_TRUE(perms_list[1]->privs.hasPermission(DashboardPrivileges::VIEW_DASHBOARD));
  ASSERT_TRUE(perms_list[1]->privs.hasPermission(DashboardPrivileges::EDIT_DASHBOARD));

  ASSERT_NO_THROW(dash_priv.add(AccessPrivileges::VIEW_DASHBOARD));
  ASSERT_NO_THROW(dash_object.setPrivileges(dash_priv));
  ASSERT_NO_THROW(sys_cat.revokeDBObjectPrivileges("Gunners", dash_object, g_cat));
  perms_list = sys_cat.getMetadataForObject(
      g_cat.get_currentDB().dbId, static_cast<int>(DBObjectType::DashboardDBObjectType), id);
  int recs4 = static_cast<int>(perms_list.size());
  ASSERT_EQ(recs4, 1);
  ASSERT_TRUE(perms_list[0]->privs.hasPermission(DashboardPrivileges::VIEW_DASHBOARD));
  ASSERT_TRUE(perms_list[0]->privs.hasPermission(DashboardPrivileges::EDIT_DASHBOARD));

  ASSERT_NO_THROW(dash_priv.add(AccessPrivileges::EDIT_DASHBOARD));
  ASSERT_NO_THROW(dash_object.setPrivileges(dash_priv));
  ASSERT_NO_THROW(sys_cat.revokeDBObjectPrivileges("Chelsea", dash_object, g_cat));
  perms_list = sys_cat.getMetadataForObject(
      g_cat.get_currentDB().dbId, static_cast<int>(DBObjectType::DashboardDBObjectType), id);
  int recs5 = static_cast<int>(perms_list.size());
  ASSERT_EQ(recs1, recs5);
  ASSERT_EQ(recs5, 0);
}

void create_tables(std::string prefix, int max) {
  auto& cat = g_session->get_catalog();
  for (int i = 0; i < max; i++) {
    std::string name = prefix + std::to_string(i);
    run_ddl_statement("CREATE TABLE " + name + " (id integer);");
    auto td = cat.getMetadataForTable(name, false);
    ASSERT_TRUE(td);
    ASSERT_EQ(td->isView, false);
    ASSERT_EQ(td->tableName, name);
  }
}

void create_views(std::string prefix, int max) {
  auto& cat = g_session->get_catalog();
  for (int i = 0; i < max; i++) {
    std::string name = "view_" + prefix + std::to_string(i);
    run_ddl_statement("CREATE VIEW " + name + " AS SELECT * FROM " + prefix + std::to_string(i) + ";");
    auto td = cat.getMetadataForTable(name, false);
    ASSERT_TRUE(td);
    ASSERT_EQ(td->isView, true);
    ASSERT_EQ(td->tableName, name);
  }
}

void create_dashboards(std::string prefix, int max) {
  auto& cat = g_session->get_catalog();
  for (int i = 0; i < max; i++) {
    std::string name = "dash_" + prefix + std::to_string(i);
    FrontendViewDescriptor vd;
    vd.viewName = name;
    vd.viewState = name;
    vd.imageHash = name;
    vd.viewMetadata = name;
    vd.userId = g_session->get_currentUser().userId;
    ASSERT_EQ(0, g_session->get_currentUser().userId);
    vd.user = g_session->get_currentUser().userName;
    cat.createFrontendView(vd);

    auto fvd = cat.getMetadataForFrontendView(std::to_string(g_session->get_currentUser().userId), name);
    ASSERT_TRUE(fvd);
    ASSERT_EQ(fvd->viewName, name);
  }
}

void assert_grants(std::string prefix, int i, bool expected) {
  auto& cat = g_session->get_catalog();

  DBObject tablePermission(prefix + std::to_string(i), DBObjectType::TableDBObjectType);
  try {
    sys_cat.getDBObjectPrivileges("bob", tablePermission, cat);
  } catch (std::runtime_error& e) {
  }
  ASSERT_EQ(expected, tablePermission.getPrivileges().hasPermission(TablePrivileges::SELECT_FROM_TABLE));

  DBObject viewPermission("view_" + prefix + std::to_string(i), DBObjectType::ViewDBObjectType);
  try {
    sys_cat.getDBObjectPrivileges("bob", viewPermission, cat);
  } catch (std::runtime_error& e) {
  }
  ASSERT_EQ(expected, viewPermission.getPrivileges().hasPermission(ViewPrivileges::SELECT_FROM_VIEW));

  auto fvd = cat.getMetadataForFrontendView(std::to_string(g_session->get_currentUser().userId),
                                            "dash_" + prefix + std::to_string(i));
  DBObject dashPermission(fvd->viewId, DBObjectType::DashboardDBObjectType);
  try {
    sys_cat.getDBObjectPrivileges("bob", dashPermission, cat);
  } catch (std::runtime_error& e) {
  }
  ASSERT_EQ(expected, dashPermission.getPrivileges().hasPermission(DashboardPrivileges::VIEW_DASHBOARD));
}

void check_grant_access(std::string prefix, int max) {
  auto& cat = g_session->get_catalog();

  for (int i = 0; i < max; i++) {
    assert_grants(prefix, i, false);

    auto fvd = cat.getMetadataForFrontendView(std::to_string(g_session->get_currentUser().userId),
                                              "dash_" + prefix + std::to_string(i));
    run_ddl_statement("GRANT SELECT ON TABLE " + prefix + std::to_string(i) + " TO bob;");
    run_ddl_statement("GRANT SELECT ON VIEW view_" + prefix + std::to_string(i) + " TO bob;");
    run_ddl_statement("GRANT VIEW ON DASHBOARD " + std::to_string(fvd->viewId) + " TO bob;");
    assert_grants(prefix, i, true);

    run_ddl_statement("REVOKE SELECT ON TABLE " + prefix + std::to_string(i) + " FROM bob;");
    run_ddl_statement("REVOKE SELECT ON VIEW view_" + prefix + std::to_string(i) + " FROM bob;");
    run_ddl_statement("REVOKE VIEW ON DASHBOARD " + std::to_string(fvd->viewId) + " FROM bob;");
    assert_grants(prefix, i, false);
  }
}

void drop_dashboards(std::string prefix, int max) {
  auto& cat = g_session->get_catalog();

  for (int i = 0; i < max; i++) {
    std::string name = "dash_" + prefix + std::to_string(i);

    cat.deleteMetadataForFrontendView(std::to_string(g_session->get_currentUser().userId), name);
    auto fvd = cat.getMetadataForFrontendView(std::to_string(g_session->get_currentUser().userId), name);
    ASSERT_FALSE(fvd);
  }
}

void drop_views(std::string prefix, int max) {
  auto& cat = g_session->get_catalog();

  for (int i = 0; i < max; i++) {
    std::string name = "view_" + prefix + std::to_string(i);
    run_ddl_statement("DROP VIEW " + name + ";");
    auto td = cat.getMetadataForTable(name, false);
    ASSERT_FALSE(td);
  }
}

void drop_tables(std::string prefix, int max) {
  auto& cat = g_session->get_catalog();

  for (int i = 0; i < max; i++) {
    std::string name = prefix + std::to_string(i);
    run_ddl_statement("DROP TABLE " + name + ";");
    auto td = cat.getMetadataForTable(name, false);
    ASSERT_FALSE(td);
  }
}

void run_concurrency_test(std::string prefix, int max) {
  create_tables(prefix, max);
  create_views(prefix, max);
  create_dashboards(prefix, max);
  check_grant_access(prefix, max);
  drop_dashboards(prefix, max);
  drop_views(prefix, max);
  drop_tables(prefix, max);
}

TEST(Catalog, Concurrency) {
  run_ddl_statement("CREATE USER bob (password = 'password', is_super = 'false');");
  std::string prefix = "for_bob";

  // only a single thread at the moment!
  // because calcite access the sqlite-dbs
  // directly when started in this mode
  int num_threads = 1;
  std::vector<std::shared_ptr<std::thread>> my_threads;

  for (int i = 0; i < num_threads; i++) {
    std::string prefix = "for_bob_" + std::to_string(i) + "_";
    my_threads.push_back(std::make_shared<std::thread>(run_concurrency_test, prefix, 100));
  }

  for (auto& thread : my_threads) {
    thread->join();
  }

  run_ddl_statement("DROP USER bob;");
}

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  google::InitGoogleLogging(argv[0]);
  testing::AddGlobalTestEnvironment(new DBObjectPermissionsEnv);
  return RUN_ALL_TESTS();
}

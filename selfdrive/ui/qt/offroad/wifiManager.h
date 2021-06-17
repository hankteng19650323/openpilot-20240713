#pragma once

#include <QWidget>
#include <QtDBus>

enum class SecurityType {
  OPEN,
  WPA,
  UNSUPPORTED
};
enum class ConnectedType{
  DISCONNECTED,
  CONNECTING,
  CONNECTED
};

typedef QMap<QString, QMap<QString, QVariant>> Connection;
typedef QVector<QMap<QString, QVariant>> IpConfig;

struct Network {
  QString path;
  QString ssid;
  unsigned int strength;
  ConnectedType connected;
  SecurityType security_type;
  bool known;
};

class WifiManager : public QObject {
  Q_OBJECT

public:
  explicit WifiManager();

private:
  void sortNetworks();
  bool isKnownNetwork(const QString &ssid);
  QVector<QPair<QString, QDBusObjectPath>> listConnections();

  void connect(const Network &ssid);
  void connect(const Network &ssid, const QString &password);
  void connect(const Network &ssid, const QString &username, const QString &password);
  void disconnect();

  // Tethering functions
  bool tetheringEnabled();
  void enableTethering();
  void disableTethering();

  void addTetheringConnection();
  void activateWifiConnection(const QString &ssid);

  QString adapter;  //Path to network manager wifi-device
  QDBusConnection bus = QDBusConnection::systemBus();
  unsigned int raw_adapter_state;  //Connection status https://developer.gnome.org/NetworkManager/1.26/nm-dbus-types.html#NMDeviceState
  QString tethering_ssid;
  QString tetheringPassword = "swagswagcommma";

  bool scanning = false;
  QString active_ap;
  QVector<Network> seen_networks;
  QString ipv4_address;
  QString connecting_to_network;

  QString get_adapter();
  QString get_ipv4_address();
  QVector<QDBusObjectPath> get_active_connections();
  QByteArray getProperty(const QString &network_path, const QString &property);
  uint get_wifi_device_state();
  QDBusObjectPath pathFromSsid(const QString &ssid);

  SecurityType getSecurityType(const QString &ssid);
  ConnectedType getConnectedType(const QString &path, const QString &ssid);

  void updateNetworks();
  void connect(const QString &ssid, const QString &username, const QString &password, SecurityType security_type);
  void updateActiveAp();
  void deactivateConnection(const QString &ssid);
  void forgetNetwork(const QString &ssid);
  unsigned int get_ap_strength(const QString &network_path);

signals:
  // Callback to main UI thread
  void updateNetworking(const QVector<Network> seen_networks, const QString ipv4_address);
  void wrongPassword(const Network n);
  void successfulConnection(const QString &ssid);

public slots:
  void requestScan();
  void connectToNetwork(const Network n, const QString pass);
  void toggleTethering(const bool enabled);
  void changeTetheringPassword(const QString newPassword);

private slots:
  void state_change(unsigned int new_state, unsigned int old_state, unsigned int reason);
  void property_change(const QString &interface, const QVariantMap &props, const QStringList &invalidated_props);
};

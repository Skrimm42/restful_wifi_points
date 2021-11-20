
# Логин пароль от WiFi сети

 Читает из файла **credentials.txt** в корне SD карты JSON объект вида

```
 {
   "ssid":"someSSID",
   "password":"somePassword"
 }
```
 и пытается подключиться к указаной wifi точке доступа. В случае удачи, 
 запускается rest_server, который берет файлы в папке **prod** на SD карте.

 В случае неудачи, стартует сканирование точек доступа и запускается
 softAP на ESP32, а затем запускается rest сервер из папки **softap** 
 в корне SD.
 Необходимо подключиться телефоном к точке доступа 
 
 * ESP32_SoftAP
 * пароль esp32password


 Из корня (192.168.2.1) методом 'GET' шлется список wifi точек и их 
 rssi JSON объектом вида

```
{
  "aps":{
    "aps":[
      {
        "ssid":"someSSID1",
        "rssi":"-30"
      },
      {
        ...
      }
    ]
  }
}
```
 После того, как юзер загрузит страницу по адресу http://192.168.2.1
 (cобственный адрес ESP32), выберет нужный wifi, введет пароль от
 wifi и нажмет кнопку connect на сервер прийдет запрос 'POST'
 по адресу url: _'192.168.2.1/updpassword'_ c json 

```
 {
   "id":"1",
   "password":"somePassword"
 }
```
По id выбирается имя wifi из массива, формируется JSON (см. самый верхний код),
который будет записан в файл **credentials.txt** в корень SD.

Необходимо перезагрузить ESP32 и переподключить телефон к своей wifi сети. 
ESP32 будет подключена к выбраной точке доступа и будет запущен
rest сервер в папке **prod**.


----------------------------------------

## Установка проекта
### Front

В папке **front** лежат два проекта, 
* **start_axios** -
для получения логин/пароль от wifi точки доступа, и  
* **greetings** - 
тестовый сайт с приветствием.

Запустить терминал в папке **start_axios**, 
запустить команды 
```
npm install
npm run build
```
Появится папка **dist**, содержимое ее скопировать в папку **softap** на SD карту.


Аналогично, те же действия в папке **greetings**, содержимое папки **dist** 
скопировать в папку **prod** на SD карту.

Создать файл **credentials.txt** в корне SD c содержимым:
```
 {
   "ssid":"someSSID",
   "password":"somePassword"
 }
```
### Back

Скомпилировать и зашить в ESP32






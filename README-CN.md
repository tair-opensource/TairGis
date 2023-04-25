<div align=center>
<img src="imgs/tairgis_logo.png" width="500"/>
</div>

## 简介 [English introduction](README.md)  
TairGis 是一个 Redis Module，支持点、线、面之间相交、包含、被包含关系的判断，具有以下特点：
- 查询性能快
- 使用`RTree`存储
- 通过`GIS.SEARCH`可实现Redis`GEORADIUS`命令的功能

## 感谢
特别感谢 https://github.com/tidwall/redis-gis, TairGis 依赖 redis-gis，并修复了其部分问题。

## 快速开始
```
// 插入一个 POLYGON
127.0.0.1:6379> GIS.ADD hangzhou campus 'POLYGON ((120.028041 30.285179, 120.031203 30.28691, 120.037311 30.286239, 120.034185 30.280844))'
(integer) 1
// 获取 POLYGON
127.0.0.1:6379> GIS.GET hangzhou campus
"POLYGON((120.028041 30.285179,120.031203 30.28691,120.037311 30.286239,120.034185 30.280844))"
// 判断点 'POINT (120.031939 30.285179)' 和 POLYGON 是否包含
127.0.0.1:6379> GIS.CONTAINS hangzhou 'POINT (120.031939 30.285179)'
1) (integer) 1
2) 1) "campus"
   2) "POLYGON((120.028041 30.285179,120.031203 30.28691,120.037311 30.286239,120.034185 30.280844))"
// 判断线和 POLYGON 是否相交   
127.0.0.1:6379> GIS.INTERSECTS hangzhou 'LINESTRING (120.029622 30.288952, 120.035488 30.280056)'
1) (integer) 1
2) 1) "campus"
   2) "POLYGON((120.028041 30.285179,120.031203 30.28691,120.037311 30.286239,120.034185 30.280844))"
127.0.0.1:6379>
```

## 编译及使用
```
make
```
编译成功后，会在当前目录下产生`tairgis.so`文件。
```
./redis-server --loadmodule /path/to/tairgis.so
```

## 测试方法
修改 tests 目录下 tairgis.tcl 文件中的路径为：`set testmodule [file your_path/tairgis.so]`

将 tests 目录下 tairgis.tcl 拷贝至 redis 目录 tests 下  
```  
cp tests/tairgis.tcl your_redis_path/tests
```
将 tairgis 添加到 redis 的 test_helper.tcl 的 all_tests 中
```      
diff --git a/tests/test_helper.tcl b/tests/test_helper.tcl
index 1a5096937..d5a1ba40a 100644
--- a/tests/test_helper.tcl
+++ b/tests/test_helper.tcl
@@ -13,6 +13,7 @@ source tests/support/test.tcl
source tests/support/util.tcl

set ::all_tests {
+    tairgis
```  
在redis根目录下运行 ./runtest --single tairgis

## 客户端
| language | GitHub |
|----------|---|
| Java     |https://github.com/alibaba/alibabacloud-tairjedis-sdk|
| Python   |https://github.com/alibaba/tair-py|
| Go       |https://github.com/alibaba/tair-go|
| .Net     |https://github.com/alibaba/AlibabaCloud.TairSDK|

## API

### GIS.ADD
#### 语法及复杂度
> GIS.ADD area polygonName polygonWkt [polygonName polygonWkt ...]  
> 时间复杂度：O(log n)

#### 命令描述
> 在area中添加指定名称的多边形（可添加多个），使用WKT（Well-known text）描述。WKT是一种文本标记语言，用于描述矢量几何对象、
> 空间参照系统及空间参照系统之间的转换。

#### 参数描述
> area：一个几何概念。  
> polygonName：多边形的名称。  
> polygonWkt：多边形的描述信息，表示现实世界的经、纬度，使用WKT（Well-known text）描述，支持如下类型。
> - POINT：描述一个点的WKT信息，例如'POINT (120.086631 30.138141)'，表示该POINT位于经度120.086631，纬度30.138141。
> - LINESTRING：描述一条线的WKT信息，由两个POINT组成，例如'LINESTRING (30 10, 40 40)'。
> - POLYGON：描述一个多边形的WKT信息，由多个POINT组成，例如'POLYGON ((31 20, 29 20, 29 21, 31 31))'。  
> 说明：经度的取值范围为(-180,180)， 纬度的取值范围为(-90,90)。不支持如下集合类型：MULTIPOINT、MULTILINESTRING、MULTIPOLYGON、GEOMETRY和COLLECTION。

#### 返回值
> 执行成功：返回插入和更新成功的多边形数量。  
> 其它情况返回相应的异常信息。  

#### 示例
```
127.0.0.1:6379> GIS.ADD hangzhou campus 'POLYGON ((30 10, 40 40, 20 40, 10 20, 30 10))'
(integer) 1
127.0.0.1:6379>
```

### GIS.GET
#### 语法及复杂度
> GIS.ADD area polygonName   
> 时间复杂度：O(1)

#### 命令描述
> 获取目标area中指定多边形的WKT信息。

#### 参数描述
> area：一个几何概念。  
> polygonName：多边形的名称。

#### 返回值
> 执行成功：WKT信息。 
> area或polygonName不存在：nil。
> 其它情况返回相应的异常信息。

#### 示例
```
提前执行 GIS.ADD hangzhou campus 'POLYGON ((30 10, 40 40, 20 40, 10 20, 30 10))'命令  

127.0.0.1:6379> GIS.GET hangzhou campus
"POLYGON((30 10,40 40,20 40,10 20,30 10))"
127.0.0.1:6379>
```

### GIS.GETALL
#### 语法及复杂度
> GIS.ADD area [WITHOUTWKT]  
> 时间复杂度：O(n)

#### 命令描述
> 获取目标area中所有多边形的名称和WKT信息。如果设置了WITHOUTWKT选项，仅返回多边形的名称。

#### 参数描述
> area：一个几何概念。  
> WITHOUTWKT：用于控制是否返回多边形的WKT信息，如果加上该参数，则不返回多边形的WKT信息。

#### 返回值
> 执行成功：返回多边形名称和WKT信息，如果设置了WITHOUTWKT选项，仅返回多边形的名称。  
> area不存在：nil。  
> 其它情况返回相应的异常信息。

#### 示例
```
提前执行 GIS.ADD hangzhou campus 'POLYGON ((30 10, 40 40, 20 40, 10 20, 30 10))'命令  

127.0.0.1:6379> GIS.GETALL hangzhou
1) "campus"
2) "POLYGON((30 10,40 40,20 40,10 20,30 10))"
127.0.0.1:6379>
```

### GIS.DEL
#### 语法及复杂度
> GIS.DEL area polygonName
> 时间复杂度：O(log n)

#### 命令描述
> 删除目标area中指定的多边形。

#### 参数描述
> area：一个几何概念。  
> polygonName：多边形的名称。

#### 返回值
> 执行成功：OK。  
> area或polygonName不存在：nil。
> 其它情况返回相应的异常信息。

#### 示例
```
提前执行 GIS.ADD hangzhou campus 'POLYGON ((30 10, 40 40, 20 40, 10 20, 30 10))'命令  

127.0.0.1:6379> GIS.DEL hangzhou campus
OK
127.0.0.1:6379>
```

### GIS.CONTAINS
#### 语法及复杂度
> GIS.CONTAINS area polygonWkt [WITHOUTWKT]  
> 时间复杂度：最好O(log M n)，最差O(log n)

#### 命令描述
> 判断指定的点、线或面是否包含在目标area的多边形中，若包含，则返回目标area中命中的多边形数量与多边形信息。

#### 参数描述
> area：一个几何概念。  
> polygonWkt：指定与目标area进行比较的多边形描述信息，使用WKT（Well-known text）描述，支持如下类型。
> - POINT：描述一个点的WKT信息。  
> - LINESTRING：描述一条线的WKT信息。  
> - POLYGON：描述一个多边形的WKT信息。
> 
> WITHOUTWKT：用于控制是否返回多边形的WKT信息，如果加上该参数，则不返回多边形的WKT信息。

#### 返回值
> 执行成功：命中的多边形数量与多边形信息。  
> area不存在：empty list or set。    
> 其它情况返回相应的异常信息。

#### 示例
```
提前执行 GIS.ADD hangzhou campus 'POLYGON ((30 10, 40 40, 20 40, 10 20, 30 10))'命令  

127.0.0.1:6379> GIS.CONTAINS hangzhou 'POINT (30 11)'
1) (integer) 1
2) 1) "campus"
   2) "POLYGON((30 10,40 40,20 40,10 20,30 10))"
127.0.0.1:6379>
```

### GIS.WITHIN
#### 语法及复杂度
> GIS.WITHIN area polygonWkt [WITHOUTWKT]  
> 时间复杂度：最好O(log M n)，最差O(log n)

#### 命令描述
> 判断目标area是否包含在指定的点、线或面中，若包含，则返回目标area中命中的多边形数量与多边形信息。

#### 参数描述
> area：一个几何概念。  
> polygonWkt：指定与目标area进行比较的多边形描述信息，使用WKT（Well-known text）描述，支持如下类型。
> - POINT：描述一个点的WKT信息。
> - LINESTRING：描述一条线的WKT信息。
> - POLYGON：描述一个多边形的WKT信息。
>
> WITHOUTWKT：用于控制是否返回多边形的WKT信息，如果加上该参数，则不返回多边形的WKT信息。

#### 返回值
> 执行成功：命中的多边形数量与多边形信息。  
> area不存在：empty list or set。    
> 其它情况返回相应的异常信息。

#### 示例
```
提前执行 GIS.ADD hangzhou campus 'POLYGON ((30 10, 40 40, 20 40, 10 20, 30 10))'命令  

127.0.0.1:6379> GIS.WITHIN hangzhou 'POLYGON ((30 5, 50 50, 20 50, 5 20, 30 5))'
1) (integer) 1
2) 1) "campus"
   2) "POLYGON((30 10,40 40,20 40,10 20,30 10))"
127.0.0.1:6379>
```

### GIS.INTERSECTS
#### 语法及复杂度
> GIS.INTERSECTS area polygonWkt [WITHOUTWKT]  
> 时间复杂度：最好O(log M n)，最差O(log n)

#### 命令描述
> 判断指定的点、线或面与目标area的多边形是否相交，若相交，则返回目标area中与其相交的多边形数量与多边形信息。

#### 参数描述
> area：一个几何概念。  
> polygonWkt：指定与目标area进行比较的多边形描述信息，使用WKT（Well-known text）描述，支持如下类型。
> - POINT：描述一个点的WKT信息。
> - LINESTRING：描述一条线的WKT信息。
> - POLYGON：描述一个多边形的WKT信息。
>
> WITHOUTWKT：用于控制是否返回多边形的WKT信息，如果加上该参数，则不返回多边形的WKT信息。

#### 返回值
> 执行成功：命中的多边形数量与多边形信息。  
> area不存在：empty list or set。    
> 其它情况返回相应的异常信息。

#### 示例
```
提前执行 GIS.ADD hangzhou campus 'POLYGON ((30 10, 40 40, 20 40, 10 20, 30 10))'命令  

127.0.0.1:6379> GIS.INTERSECTS hangzhou 'LINESTRING (30 10, 40 40)'
1) (integer) 1
2) 1) "campus"
   2) "POLYGON((30 10,40 40,20 40,10 20,30 10))"
127.0.0.1:6379>
```

### GIS.SEARCH
#### 语法及复杂度
> GIS.SEARCH area [RADIUS longitude latitude distance m|km|ft|mi]  
> [MEMBER field distance m|km|ft|mi]   
> [GEOM geom]  
> [COUNT count]  
> [ASC|DESC]  
> [WITHDIST]  
> [WITHOUTWKT]  
> 时间复杂度：最好O(log M n)，最差O(log n)

#### 命令描述
> 在指定经、纬度及半径距离范围内，查找目标area中的点。

#### 参数描述
> area：一个几何概念。  
> RADIUS：传入经度（longitude）、纬度（latitude）、半径距离（distance）和半径单位（m表示米、km表示千米、ft表示英尺、mi表示英里）进行搜索，例如RADIUS 15 37 200 km。  
> MEMBER：选择当前area中已存在的POINT作为搜索原点，并指定半径进行搜索，取值顺序为多边形名称（field）、半径（distance）、半径单位（m表示米、km表示千米、ft表示英尺、mi表示英里），例如MEMBER Agrigento 100 km。
> GEOM：按照WKT的格式设置搜索范围，可以是任意多边形，例如GEOM 'POLYGON((10 30,20 30,20 40,10 40))'。  
> COUNT：用于限定返回的个数，例如COUNT 3。  
> ASC|DESC：用于控制返回信息按照距离排序，ASC表示根据中心位置，由近到远排序；DESC表示由远到近排序。  
> WITHDIST：用于控制是否返回目标点与搜索原点的距离。  
> WITHOUTWKT：用于控制是否返回多边形的WKT信息，如果加上该参数，则不返回多边形的WKT信息。
> 
> 说明:只能同时使用RADIUS、MEMBER和GEOM中的一种方式。

#### 返回值
> 执行成功：命中的目标点数量与WKT信息。    
> area不存在：empty list or set。    
> 其它情况返回相应的异常信息。

#### 示例
```
提前执行GIS.ADD Sicily "Palermo" "POINT (13.361389 38.115556)" "Catania" "POINT(15.087269 37.502669)"命令。 

127.0.0.1:6379> GIS.SEARCH Sicily RADIUS 15 37 200 km WITHDIST ASC
1) (integer) 2
2) 1) "Catania"
   2) "POINT(15.087269 37.502669)"
   3) "56.4413"
   4) "Palermo"
   5) "POINT(13.361389 38.115556)"
   6) "190.4424"
127.0.0.1:6379>
```

## Tair Modules
[TairHash](https://github.com/alibaba/TairHash): 和redis hash类似，但是可以为field设置expire和version，支持高效的主动过期和被动过期。  
[TairZset](https://github.com/alibaba/TairZset): 和redis zset类似，但是支持多（最大255）维排序，同时支持incrby语义，非常适合游戏排行榜场景。  
[TairString](https://github.com/alibaba/TairString): 和redis string类似，但是支持设置expire和version，并提供CAS/CAD等实用命令，非常适用于分布式锁等场景。  
[TairGis](https://github.com/alibaba/TairGis): 一个支持点、线、面之间相交、包含、被包含关系判断的Redis Module。  
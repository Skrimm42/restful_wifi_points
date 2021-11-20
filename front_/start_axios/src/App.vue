<template>
<div class="container">
  <div class="card center" v-if="appStatus.onError || appStatus.onMessageUpdated">
    <h2 v-if="appStatus.onError">{{appStatus.errorMessage}}</h2>
    <h2 v-if="appStatus.onMessageUpdated">{{appStatus.messageUpdated}}</h2>
  </div>
  <div class="card center">
    <h2> WIFI AP list</h2>
    <ul class="list">
      <li class="list-item" v-for="(item, index) in apList" :key=index @click="toggleVisibility(index)">
        <div class="text-ssid">{{item.ssid}}</div>
        <div class="text-strength">{{item.rssi}} dB</div>
        <div>
          <input type="text" placeholder="password..." :style="{display: item.visible ? 'block':'none'}" @click.stop v-model="wifiPassword" @keypress.enter="passInputHandler(index)"/>
          <button class="btn danger" :style="{display: item.visible ? 'block':'none'}" @click.stop="passInputHandler(index)">Connect</button>
        </div>
      </li>
    </ul>
  </div>
</div>
</template>

<script>
import axios from 'axios'
export default {
  name: 'App',
  components: {
    
  },
  data(){
    return{
      apList:null,
      wifiPassword:'',
      itemsToggleVisibility:{
        current:-1,
        previous:-1
      },
      appStatus:{
        onError:false,
        errorMessage:null,
        onMessageUpdated:false,
        messageUpdated:null
      }
      
    }
  },
  methods:{
    async passInputHandler(i){
      console.log('inputPassHandler', i ,'pass:', this.wifiPassword)
      let updPass={
        id:i,
        password:this.wifiPassword
      }
      try{
        const postResponse = await axios.post('http://192.168.2.1/updpassword', updPass)
        if(!postResponse){
          this.appStatus.onError=true
          this.appStatus.errorMessage='Didn\'t get any response' 
        }
        else{
          this.appStatus.onError=false
          this.appStatus.onMessageUpdated=true
          this.appStatus.messageUpdated='Password was updated successfully.'
        }
        console.log('POST response: ', postResponse)
        if(!postResponse)throw new Error('Unable to update password')
      } catch(e){
        this.appStatus.onError=true
        this.appStatus.errorMessage='Something went wrong: '+ e
        console.log(this.appStatus.errorMessage)
      } 
    }, 

    toggleVisibility(i){
      if(this.itemsToggleVisibility.previous == -1 && this.itemsToggleVisibility.current == -1){
        this.itemsToggleVisibility.current = i
        this.itemsToggleVisibility.previous = i
      }
      else{
        this.itemsToggleVisibility.previous = this.itemsToggleVisibility.current
        this.itemsToggleVisibility.current = i
      } 

      if(this.itemsToggleVisibility.previous == this.itemsToggleVisibility.current){
        this.apList[this.itemsToggleVisibility.current].visible = true
        this.wifiPassword=''
      }
      else{
        this.apList[this.itemsToggleVisibility.current].visible = true
        this.apList[this.itemsToggleVisibility.previous].visible = false
        this.wifiPassword=''
      }
    }
  },

  async mounted(){
    console.log('mounted')
    try{
      const {data} = await axios.get('http://192.168.2.1/aps')
      //const {data} = await axios.get('http://localhost:3000/aps')
      if(!data)throw new Error('Wifi list is empty')
      if(!data.hasOwnProperty('aps'))throw new Error('Wrong format response from server')
      this.apList=data.aps
      console.log('Data fetched: ',this.apList)
    } catch(e){
      this.appStatus.onError=true
      this.appStatus.errorMessage='Something went wrong: '+ e
      console.log(this.appStatus.errorMessage)
    } 
  }

}
</script>

